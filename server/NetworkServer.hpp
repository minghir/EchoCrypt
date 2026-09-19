#ifndef NETWORKSERVER_HPP
#define NETWORKSERVER_HPP

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <thread>
#include <vector>
#include <mutex>
#include <map>
#include <string>

#include "ConsoleManager.hpp"
#include "GameEngine.hpp"
#include "db/dbConnection.hpp"

#pragma comment(lib, "ws2_32.lib")

enum class UserRole { ADMIN, USER };

struct ActiveSession {
    std::wstring username;
    std::wstring ip;
    UserRole role;
};

class NetworkServer {
private:
    SOCKET listenSocket = INVALID_SOCKET;
    bool m_running = false;

    std::map<SOCKET, ActiveSession> m_activeSessions;
    std::mutex m_sessionsMutex;

    // Helper-e pentru I/O pe rețea
    std::wstring receiveLine(SOCKET clientSocket);
    void sendText(SOCKET clientSocket, const std::wstring& text);

    // Autentificare direct pe DB
    bool authenticateUser(dbConnection& db, const std::wstring& user, const std::wstring& pass, UserRole& outRole);
    bool registerUser(dbConnection& db, const std::wstring& user, const std::wstring& pass);

    void handleClient(SOCKET clientSocket, dbConnection& db, GameEngine& engine);

public:
    NetworkServer();
    ~NetworkServer();

    bool init(int port);
    void run(dbConnection& db, GameEngine& engine);
};

#endif // NETWORKSERVER_HPP