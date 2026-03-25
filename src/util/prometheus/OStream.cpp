#include "util/prometheus/OStream.hpp"

#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/filter/gzip.hpp>
#include <boost/iostreams/filter/zlib.hpp>

#include <string>
#include <utility>

namespace util::prometheus {
OStream::OStream(bool const compressionEnabled) : compressionEnabled_(compressionEnabled)
{
    if (compressionEnabled_) {
        stream_.push(
            boost::iostreams::gzip_compressor{
                boost::iostreams::gzip_params{boost::iostreams::gzip::best_compression}
            }
        );
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
