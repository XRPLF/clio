//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#include <util/prometheus/OStream.h>

#include <boost/iostreams/filter/gzip.hpp>

namespace util::prometheus {
OStream::OStream(bool const compressionEnabled) : compressionEnabled_(compressionEnabled)
{
    if (compressionEnabled_) {
        stream_.push(boost::iostreams::gzip_compressor{
            boost::iostreams::gzip_params{boost::iostreams::gzip::best_compression}});
    }
    stream_.push(boost::iostreams::back_inserter(buffer_));
}

std::string
OStream::data() &&
{
    stream_.reset();
    return std::move(buffer_);
}

}  // namespace util::prometheus
