#include "util/AssignRandomPort.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <cstdint>

using tcp = boost::asio::ip::tcp;

namespace tests::util {

uint32_t
generateFreePort()
{
    boost::asio::io_context ioContext;
    tcp::acceptor acceptor(ioContext);
    tcp::endpoint const endpoint(tcp::v4(), 0);

    acceptor.open(endpoint.protocol());
    acceptor.set_option(tcp::acceptor::reuse_address(true));
    acceptor.bind(endpoint);

    return acceptor.local_endpoint().port();
}

}  // namespace tests::util
