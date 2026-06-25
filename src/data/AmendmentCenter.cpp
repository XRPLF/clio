#include "data/AmendmentCenter.hpp"

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/digest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::unordered_set<std::string>&
supportedAmendments()
{
    static std::unordered_set<std::string> kAmendments = {};
    return kAmendments;
}

bool
lookupAmendment(
    auto const& allAmendments,
    std::vector<xrpl::uint256> const& ledgerAmendments,
    std::string_view name
)
{
    namespace rg = std::ranges;
    if (auto const am = rg::find(allAmendments, name, &data::Amendment::name);
        am != rg::end(allAmendments))
        return rg::find(ledgerAmendments, am->feature) != rg::end(ledgerAmendments);
    return false;
}

}  // namespace

namespace data {
namespace impl {

WritingAmendmentKey::WritingAmendmentKey(std::string amendmentName)
    : AmendmentKey{std::move(amendmentName)}
{
    ASSERT(
        not supportedAmendments().contains(name), "Attempt to register the same amendment twice"
    );
    supportedAmendments().insert(name);
}

}  // namespace impl

AmendmentKey::
operator std::string const&() const
{
    return name;
}

AmendmentKey::
operator std::string_view() const
{
    return name;
}

AmendmentKey::
operator xrpl::uint256() const
{
    return Amendment::getAmendmentId(name);
}

AmendmentCenter::AmendmentCenter(std::shared_ptr<data::BackendInterface> const& backend)
    : backend_{backend}
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    rg::copy(
        xrpl::allAmendments() | vs::transform([&](auto const& p) {
            auto const& [name, support] = p;
            return Amendment{
                .name = name,
                .feature = Amendment::getAmendmentId(name),
                .isSupportedByXRPL = support != xrpl::AmendmentSupport::Unsupported,
                .isSupportedByClio = rg::contains(supportedAmendments(), name),
                .isRetired = support == xrpl::AmendmentSupport::Retired
            };
        }),
        std::back_inserter(all_)
    );

    // Mix in amendments that Clio registered but libXRPL no longer knows about (deleted upstream).
    for (auto const& name : supportedAmendments()) {
        if (not rg::contains(all_, name, &Amendment::name)) {
            all_.push_back({
                .name = name,
                .feature = Amendment::getAmendmentId(name),
                .isSupportedByXRPL = false,
                .isSupportedByClio = true,
                .isRetired = true,
            });
        }
    }

    for (auto const& am : all_ | vs::filter([](auto const& am) { return am.isSupportedByClio; }))
        supported_.insert_or_assign(am.name, am);
}

bool
AmendmentCenter::isSupported(AmendmentKey const& key) const
{
    return supported_.contains(key);
}

std::map<std::string, Amendment> const&
AmendmentCenter::getSupported() const
{
    return supported_;
}

std::vector<Amendment> const&
AmendmentCenter::getAll() const
{
    return all_;
}

bool
AmendmentCenter::isEnabled(AmendmentKey const& key, uint32_t seq) const
{
    return data::synchronous([this, &key, seq](auto yield) { return isEnabled(yield, key, seq); });
}

bool
AmendmentCenter::isEnabled(
    boost::asio::yield_context yield,
    AmendmentKey const& key,
    uint32_t seq
) const
{
    try {
        if (auto const listAmendments = fetchAmendmentsList(yield, seq); listAmendments)
            return lookupAmendment(all_, *listAmendments, key);
    } catch (std::runtime_error const&) {
        return false;  // Some old ledger does not contain Amendments ledger object so do best we
                       // can for now
    }
    return false;
}

std::vector<bool>
AmendmentCenter::isEnabled(
    boost::asio::yield_context yield,
    std::vector<AmendmentKey> const& keys,
    uint32_t seq
) const
{
    namespace rg = std::ranges;

    try {
        if (auto const listAmendments = fetchAmendmentsList(yield, seq); listAmendments) {
            std::vector<bool> out;
            rg::transform(keys, std::back_inserter(out), [this, &listAmendments](auto const& key) {
                return lookupAmendment(all_, *listAmendments, key);
            });

            return out;
        }
    } catch (std::runtime_error const&) {
        return std::vector<bool>(
            keys.size(), false
        );  // Some old ledger does not contain Amendments ledger object so do best we can for now
    }

    return std::vector<bool>(keys.size(), false);
}

Amendment const&
AmendmentCenter::getAmendment(AmendmentKey const& key) const
{
    ASSERT(
        supported_.contains(key),
        "The amendment '{}' must be present in supported amendments list",
        key.name
    );
    return supported_.at(key);
}

Amendment const&
AmendmentCenter::operator[](AmendmentKey const& key) const
{
    return getAmendment(key);
}

xrpl::uint256
Amendment::getAmendmentId(std::string_view name)
{
    return xrpl::sha512Half(xrpl::Slice(name.data(), name.size()));
}

std::optional<std::vector<xrpl::uint256>>
AmendmentCenter::fetchAmendmentsList(boost::asio::yield_context yield, uint32_t seq) const
{
    // the amendments should always be present on the ledger
    auto const amendments = backend_->fetchLedgerObject(xrpl::keylet::amendments().key, seq, yield);
    if (not amendments.has_value())
        throw std::runtime_error("Amendments ledger object must be present in the database");

    xrpl::SLE const amendmentsSLE{
        xrpl::SerialIter{amendments->data(), amendments->size()}, xrpl::keylet::amendments().key
    };

    return amendmentsSLE[~xrpl::sfAmendments];
}

}  // namespace data
