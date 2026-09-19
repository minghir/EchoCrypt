#pragma once

#include "network/vNetworkServer.hpp"
#include "ui/vApp.hpp"
#include "ConfigLoader.hpp"

#include "EchoCryptEngine.hpp"
#include "RedisClient.hpp"
#include "PgClient.hpp"

#include <mutex>

std::mutex engineMutex;

class GameServer : public vApp {
    int port;
    std::wstring config_file;

public:
    GameServer(HINSTANCE hInstance, RunMode rm, int srv_port, std::wstring cfg)
        : vApp(hInstance), port(srv_port), config_file(cfg)
    {
        setRunMode(rm);
    }

    bool initService() override {
        // Load config
        ConfigLoader loader;
        if (!loader.load(config_file)) {
            LOG_WARN(L"[CONFIG] Using default values.");
        }

        // Init PostgreSQL
        PgClient pg(
            loader.get(L"pg_host", L"localhost"),
            loader.get(L"pg_user", L"postgres"),
            loader.get(L"pg_pass", L""),
            loader.get(L"pg_db", L"echocrypt")
        );

        if (!pg.connect()) {
            LOG_FATAL(L"[PG] Cannot connect to PostgreSQL.");
            return false;
        }

        // Init Redis
        RedisClient redis(
            loader.get(L"redis_host", L"127.0.0.1"),
            loader.get(L"redis_port", L"6379")
        );

        if (!redis.connect()) {
            LOG_FATAL(L"[REDIS] Cannot connect to Redis.");
            return false;
        }

        // Create game engine
        EchoCryptEngine engine(pg, redis);

        // Init network server
        vNetworkServer server;
        server.loadUsers(loader.get(L"users_file", L"echocrypt_users.pwd"));

        LOG_INFO(L"[SERVER] Starting EchoCrypt server...");
        if (!server.init(port)) {
            LOG_FATAL(L"[SERVER] Cannot bind port: " + std::to_wstring(port));
            return false;
        }

        // Run server (blocking)
        server.run(engine);

        return true;
    }
};
