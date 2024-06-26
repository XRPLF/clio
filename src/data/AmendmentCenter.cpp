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
#include <fmt/compile.h>
#include <ripple/basics/Slice.h>
#include <ripple/basics/base_uint.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>
#include <ripple/protocol/digest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::string_view> SUPPORTED_AMENDMENTS = {};
}  // namespace

namespace data {

AmendmentKey::AmendmentKey(std::string_view amendmentName) : name(amendmentName)
{
    SUPPORTED_AMENDMENTS.push_back(amendmentName);
}

AmendmentKey::AmendmentKey(std::string const& amendmentName) : name(amendmentName)
{
    SUPPORTED_AMENDMENTS.push_back(amendmentName);
}

AmendmentKey::operator std::string() const
{
    return std::string{name};
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
                .isSupportedByXRPL = p.second != ripple::AmendmentSupport::Unsupported,
                .isSupportedByClio = rg::find(SUPPORTED_AMENDMENTS, p.first) != rg::end(SUPPORTED_AMENDMENTS),
                .isRetired = p.second == ripple::AmendmentSupport::Retired
            };
        }),
        std::back_inserter(all_)
    );

    for (auto const& am : all_ | vs::filter([](auto const& am) { return am.isSupportedByClio; }))
        supported_.insert_or_assign(am.name, am);
}

bool
AmendmentCenter::isSupported(std::string name) const
{
    return supported_.contains(name) && supported_.at(name).isSupportedByClio;
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
AmendmentCenter::isEnabled(std::string name, uint32_t seq) const
{
    return data::synchronous([this, name, seq](auto yield) { return isEnabled(yield, name, seq); });
}

bool
AmendmentCenter::isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const
{
    namespace rg = std::ranges;

    // the amendments should always be present in ledger
    auto const& amendments = backend_->fetchLedgerObject(ripple::keylet::amendments().key, seq, yield);

    ripple::SLE const amendmentsSLE{
        ripple::SerialIter{amendments->data(), amendments->size()}, ripple::keylet::amendments().key
    };

    auto const listAmendments = amendmentsSLE.getFieldV256(ripple::sfAmendments);

    if (auto am = rg::find(all_, key.name, [](auto const& am) { return am.name; }); am != rg::end(all_)) {
        return rg::find(listAmendments, am->feature) != rg::end(listAmendments);
    }

    return false;
}

Amendment const&
AmendmentCenter::getAmendment(std::string const& name) const
{
    ASSERT(supported_.contains(name), "The amendment '{}' must be present in supported amendments list", name);
    return supported_.at(name);
}

Amendment const&
AmendmentCenter::operator[](AmendmentKey const& key) const
{
    return getAmendment(key);
}

ripple::uint256
Amendment::GetAmendmentId(std::string_view const name)
{
    return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
}

}  // namespace data
