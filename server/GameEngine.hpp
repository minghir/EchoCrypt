#pragma once

#include "db/dbConnection.hpp"
#include <string>
#include <mutex>

// Structură internă pentru menținerea stării curente a jucătorului
struct PlayerState {
    int userId = 0;
    std::wstring username;
    int level = 1;
    int hp = 100;
    int maxHp = 100;
    int x = 0;
    int y = 0;
    int dungeonId = 1;
};

class GameEngine {
private:
    dbConnection& m_db;
    mutable std::mutex m_engineMutex;

    // Aici vom adăuga ulterior:
    // RedisClient& m_redis;
    // AIGameMaster m_aiGM;

    // Helper-e interne pentru interacțiunea cu PostgreSQL
    PlayerState getOrInitPlayerState(const std::wstring& username);
    bool updatePlayerPosition(int userId, int newX, int newY);
    std::wstring getRoomDescription(int dungeonId, int x, int y, std::wstring& outSymbol);

public:
    explicit GameEngine(dbConnection& db);
    ~GameEngine() = default;

    // Interfața principală: primește comanda de la client și utilizatorul conectat
    std::wstring processRequest(const std::wstring& req, const std::wstring& user);
};