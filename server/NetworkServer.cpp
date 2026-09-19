#include "NetworkServer.hpp"
#include "stringUtils.hpp"
#include <sstream>

NetworkServer::NetworkServer() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

NetworkServer::~NetworkServer() {
    m_running = false;
    if (listenSocket != INVALID_SOCKET) {
        closesocket(listenSocket);
    }
    WSACleanup();
}

bool NetworkServer::init(int port) {
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) return false;

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
        return false;

    listen(listenSocket, SOMAXCONN);
    m_running = true;
    return true;
}

void NetworkServer::sendText(SOCKET clientSocket, const std::wstring& text) {
    std::string utf8 = wstr_to_str(text) + "\n";
    send(clientSocket, utf8.c_str(), static_cast<int>(utf8.length()), 0);
}

std::wstring NetworkServer::receiveLine(SOCKET clientSocket) {
    std::string line;
    char ch;
    while (true) {
        int r = recv(clientSocket, &ch, 1, 0);
        if (r <= 0) break;
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return str_to_wstr(line);
}

// Interogare PostgreSQL pentru Login
bool NetworkServer::authenticateUser(dbConnection& db, const std::wstring& user, const std::wstring& pass, UserRole& outRole) {
    db.clearStatement("auth_stmt");

    std::wstring query = L"SELECT role FROM echocrypt.users WHERE username = $1 AND password_hash = $2";
    std::vector<std::wstring> params = { user, pass };

    bool success = false;
    if (db.execQuery(query, params, "auth_stmt")) {
        if (db.fetchNextRow("auth_stmt")) {
            std::wstring roleStr = db.fetchFieldByNumber(1, "auth_stmt");
            outRole = (roleStr == L"ADMIN") ? UserRole::ADMIN : UserRole::USER;
            success = true;
        }
    }
    db.clearStatement("auth_stmt");
    return success;
}

// Inregistrare utilizator nou în Postgres
bool NetworkServer::registerUser(dbConnection& db, const std::wstring& user, const std::wstring& pass) {
    std::wstring query = L"INSERT INTO echocrypt.users (username, password_hash) VALUES ($1, $2)";
    std::vector<std::wstring> params = { user, pass };

    bool success = db.execQuery(query, params, "reg_stmt");
    db.clearStatement("reg_stmt");
    return success;
}

void NetworkServer::run(dbConnection& db, GameEngine& engine) {
    while (m_running) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        std::thread([this, clientSocket, &db, &engine]() {
            this->handleClient(clientSocket, db, engine);
            }).detach();
    }
}

void NetworkServer::handleClient(SOCKET clientSocket, dbConnection& db, GameEngine& engine) {
    // Obținem IP-ul clientului
    sockaddr_in addr;
    int addrLen = sizeof(addr);
    getpeername(clientSocket, (sockaddr*)&addr, &addrLen);
    char ipBuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &addr.sin_addr, ipBuf, INET_ADDRSTRLEN);
    std::wstring clientIP = str_to_wstr(ipBuf);

    // --- ETAPA 1: AUTENTIFICARE / ÎNREGISTRARE ---
    sendText(clientSocket, L"--- WELCOME TO ECHOCRYPT ---");
    sendText(clientSocket, L"1. Login\n2. Register\nAlege o optiune (1/2):");

    std::wstring opt = receiveLine(clientSocket);
    sendText(clientSocket, L"User:");
    std::wstring user = receiveLine(clientSocket);
    sendText(clientSocket, L"Pass:");
    std::wstring pass = receiveLine(clientSocket);

    UserRole role = UserRole::USER;
    bool authenticated = false;

    if (opt == L"2") { // Inregistrare
        if (registerUser(db, user, pass)) {
            sendText(clientSocket, L"Cont creat cu succes! Te conectam...");
            authenticated = authenticateUser(db, user, pass, role);
        }
        else {
            sendText(clientSocket, L"Eroare: Numele de utilizator exista deja sau date invalide.");
            closesocket(clientSocket);
            return;
        }
    }
    else { // Login
        authenticated = authenticateUser(db, user, pass, role);
    }

    if (!authenticated) {
        LOG_ERROR(L"[" + clientIP + L"] Autentificare esuata pentru: " + user);
        sendText(clientSocket, L"Eroare: Utilizator sau parola gresita!");
        closesocket(clientSocket);
        return;
    }

    LOG_SUCCESS(L"[" + clientIP + L"] Utilizatorul '" + user + L"' s-a conectat.");
    sendText(clientSocket, L"Login reusit! Bun venit in EchoCrypt.");

    // Salvare sesiune activă
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_activeSessions[clientSocket] = { user, clientIP, role };
    }

    // --- ETAPA 2: LOOP DE JOC ---
    bool sessionActive = true;
    while (sessionActive) {
        std::wstring command = receiveLine(clientSocket);
        if (command.empty()) break; // Conexiune închisă de client

        if (command == L"/exit" || command == L"/quit") {
            sendText(clientSocket, L"La revedere!");
            break;
        }

        // Comenzi speciale de admin
        if (command == L"/sessions" && role == UserRole::ADMIN) {
            std::lock_guard<std::mutex> lock(m_sessionsMutex);
            sendText(clientSocket, L"=== SESIUNI ACTIVE ===");
            for (const auto& [s, sess] : m_activeSessions) {
                sendText(clientSocket, sess.username + L" (" + sess.ip + L")");
            }
            continue;
        }

        // Procesare comanda normala de joc via GameEngine
        std::wstring response = engine.processRequest(command, user);
        sendText(clientSocket, response);
    }

    // Deconectare
    {
        std::lock_guard<std::mutex> lock(m_sessionsMutex);
        m_activeSessions.erase(clientSocket);
    }
    LOG_INFO(L"[" + clientIP + L"] Sesiune incheiata pentru: " + user);
    closesocket(clientSocket);
}