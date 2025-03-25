//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

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

#pragma once

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/uuid/uuid.hpp>

#include <chrono>
#include <memory>

namespace cluster {

struct ClioNode {
    static constexpr char const* kTIME_FORMAT = "%Y-%m-%dT%H:%M:%SZ";

    // enum class WriterRole {
    //     ReadOnly,
    //     NotWriter,
    //     Writer
    // };

    std::shared_ptr<boost::uuids::uuid> uuid;
    std::chrono::system_clock::time_point updateTime;
    bool isSelf;
    // WriterRole writerRole;
};

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, ClioNode const& node);

ClioNode
tag_invoke(boost::json::value_to_tag<ClioNode>, boost::json::value const& jv);

}  // namespace cluster
