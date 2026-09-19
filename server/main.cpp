#include "GameServer.hpp"
#include "GameEngine.hpp"

int main(int argc, char* argv[]) {
    std::string configPath = "./echocrypt.cfg";
    if (argc > 1) {
        configPath = argv[1];
    }

    GameServer app(NULL, RunMode::SERVICE, 3519, std::wstring(configPath.begin(), configPath.end()));
    app.startConsole();
    return app.run();
}