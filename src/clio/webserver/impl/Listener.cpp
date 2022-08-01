#include <clio/webserver/Listener.h>

namespace Server {

std::shared_ptr<HttpServer>
make_HttpServer(Application const& app)
{
    if (!app.config().server)
        return nullptr;

    auto server = std::make_shared<Server::HttpServer>(app);

    server->run();

    return server;
}
}  // namespace Server