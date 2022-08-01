#include <cstdlib>
#include <iostream>

#include <clio/main/Application.h>
#include <clio/main/Config.h>

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 2)
    {
        std::cerr << "Usage: clio_server "
                     "<config_file> \n"
                  << "Example:\n"
                  << "    clio_server config.json \n";
        return EXIT_FAILURE;
    }

    auto configJson = parseConfig(argv[1]);

    auto app = make_Application(std::make_unique<Config>(configJson));

    app->start();

    return EXIT_SUCCESS;
}