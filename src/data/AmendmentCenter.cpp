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
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <fmt/compile.h>
#include <ripple/protocol/Feature.h>
#include <ripple/protocol/Indexes.h>
#include <ripple/protocol/SField.h>
#include <ripple/protocol/STLedgerEntry.h>
#include <ripple/protocol/Serializer.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string>
#include <unordered_map>
#include <vector>

namespace data {

std::vector<Amendment>
xrplAmendments()
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    std::vector<Amendment> amendments;

    // TODO: some day we will be able to do | std::to<std::vector>() but gcc still got some catching up to do
    rg::copy(
        ripple::detail::supportedAmendments() | vs::transform([&](auto const& p) { return Amendment{p.first}; }),
        std::back_inserter(amendments)
    );

    return amendments;
}

bool
AmendmentCenter::isSupported(std::string name) const
{
    return supported_.contains(name) && supported_.at(name).supportedByClio;
}

std::unordered_map<std::string, Amendment> const&
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
AmendmentCenter::isEnabled(boost::asio::yield_context yield, std::string name, uint32_t seq) const
{
    namespace rg = std::ranges;

    // the amendments should always be present in ledger
    auto const& amendments = backend_->fetchLedgerObject(ripple::keylet::amendments().key, seq, yield);

    ripple::SLE const amendmentsSLE{
        ripple::SerialIter{amendments->data(), amendments->size()}, ripple::keylet::amendments().key
    };

    auto const listAmendments = amendmentsSLE.getFieldV256(ripple::sfAmendments);

    if (auto am = rg::find(all_, name, [](auto const& am) { return am.name; }); am != rg::end(all_)) {
        return rg::find(listAmendments, am->feature) != rg::end(listAmendments);
    }

    return false;
}

Amendment const&
AmendmentCenter::getAmendment(std::string name) const
{
    // todo: fix string contains \0
    ASSERT(supported_.contains(name.data()), "The amendment '{}' must be present in supported amendments list", name);
    return supported_.at(name.data());
}

}  // namespace data
