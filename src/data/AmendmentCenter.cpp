//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

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
#include <xrpl/protocol/STVector256.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/digest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::unordered_set<std::string>&
SUPPORTED_AMENDMENTS()
{
    static std::unordered_set<std::string> amendments = {};
    return amendments;
}

}  // namespace

namespace data {
namespace impl {

WritingAmendmentKey::WritingAmendmentKey(std::string amendmentName) : AmendmentKey{std::move(amendmentName)}
{
    ASSERT(not SUPPORTED_AMENDMENTS().contains(name), "Attempt to register the same amendment twice");
    SUPPORTED_AMENDMENTS().insert(name);
}

}  // namespace impl

AmendmentKey::operator std::string const&() const
{
    return name;
}

AmendmentKey::operator ripple::uint256() const
{
    return Amendment::GetAmendmentId(name);
}

AmendmentCenter::AmendmentCenter(std::shared_ptr<data::BackendInterface> const& backend) : backend_{backend}
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    rg::copy(
        ripple::allAmendments() | vs::transform([&](auto const& p) {
            auto const& [name, support] = p;
            return Amendment{
                .name = name,
                .feature = Amendment::GetAmendmentId(name),
                .isSupportedByXRPL = support != ripple::AmendmentSupport::Unsupported,
                .isSupportedByClio = rg::find(SUPPORTED_AMENDMENTS(), name) != rg::end(SUPPORTED_AMENDMENTS()),
                .isRetired = support == ripple::AmendmentSupport::Retired
            };
        }),
        std::back_inserter(all_)
    );

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
AmendmentCenter::isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const
{
    namespace rg = std::ranges;

    auto const listAmendments = fetchAmendmentsList(yield, seq);
    if (listAmendments) {
        if (auto am = rg::find(all_, key.name, [](auto const& am) { return am.name; }); am != rg::end(all_))
            return rg::find(*listAmendments, am->feature) != rg::end(*listAmendments);
    }

    return false;
}

std::vector<bool>
AmendmentCenter::isEnabled(boost::asio::yield_context yield, std::vector<AmendmentKey> const& keys, uint32_t seq) const
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    auto const listAmendments = fetchAmendmentsList(yield, seq);
    if (not listAmendments)
        return std::vector<bool>(keys.size(), false);

    return keys  //
        | vs::transform([this, &listAmendments](auto const& key) {
               if (auto am = rg::find(all_, key.name, [](auto const& am) { return am.name; }); am != rg::end(all_))
                   return rg::find(*listAmendments, am->feature) != rg::end(*listAmendments);
               return false;
           })  //
        | rg::to<std::vector>();
}

Amendment const&
AmendmentCenter::getAmendment(AmendmentKey const& key) const
{
    ASSERT(supported_.contains(key), "The amendment '{}' must be present in supported amendments list", key.name);
    return supported_.at(key);
}

Amendment const&
AmendmentCenter::operator[](AmendmentKey const& key) const
{
    return getAmendment(key);
}

ripple::uint256
Amendment::GetAmendmentId(std::string_view name)
{
    return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
}

std::optional<ripple::STVector256 const>
AmendmentCenter::fetchAmendmentsList(boost::asio::yield_context yield, uint32_t seq) const
{
    // the amendments should always be present on the ledger
    auto const& amendments = backend_->fetchLedgerObject(ripple::keylet::amendments().key, seq, yield);
    ASSERT(amendments.has_value(), "Amendments ledger object must be present in the database");

    ripple::SLE const amendmentsSLE{
        ripple::SerialIter{amendments->data(), amendments->size()}, ripple::keylet::amendments().key
    };

    if (not amendmentsSLE.isFieldPresent(ripple::sfAmendments))
        return std::nullopt;

    return amendmentsSLE.getFieldV256(ripple::sfAmendments);
}

}  // namespace data
