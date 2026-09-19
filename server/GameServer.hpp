#pragma once
#include "NetworkServer.hpp"
#include "App.hpp"
#include "ConfigLoader.hpp"
#include "GameEngine.hpp"
#include "db/pgConnection.hpp"
#include <memory>

class GameServer : public App {
    int port;
    std::string config_file;

public:
    GameServer(HINSTANCE hInstance, RunMode rm, int srv_port, std::string cfg)
        : App(), port(srv_port), config_file(cfg)
    {
        setRunMode(rm);
    }

    bool initService() override {
        ConfigLoader loader;
        loader.load(config_file);

        std::wstring dsn = L"host=localhost port=5432 dbname=echocrypt_db user=postgres password=postgres";

        // Conexiunea la PostgreSQL
        auto pg = std::make_unique<pgConnection>("pgsql", dsn);
        if (!pg->openDatabase()) {
            LOG_FATAL(L"[PG] Cannot connect to PostgreSQL.");
            return false;
        }

        addConnection("pg_main", std::move(pg));
        dbConnection* db = getConnection("pg_main");

        // Motorul de joc
        GameEngine engine(*db);

        // Serverul de rețea
        NetworkServer server;
        LOG_INFO(L"[SERVER] Starting EchoCrypt server on port " + std::to_wstring(port) + L"...");

        if (!server.init(port)) {
            LOG_FATAL(L"[SERVER] Cannot bind port: " + std::to_wstring(port));
            return false;
        }

        // Pornim bucla (sincronă)
        server.run(*db, engine);

        return true;
    }
};