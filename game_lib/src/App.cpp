#include "App.hpp"

// 1. Inițializarea pointerului static singleton
App* App::s_instance = nullptr;

// 2. Constructorul
App::App(RunMode mode) : m_runMode(mode) {
    s_instance = this;
}

// 3. Destructorul
App::~App() {
    closeAllConnections();
}

// 4. Metoda init()
bool App::init() {
    if (m_runMode == RunMode::CONSOLE) {
        return initConsole();
    }
    else {
        return initService();
    }
}

// 5. Metoda startConsole()
void App::startConsole() {
    LOG_INFO(L"[APP] Starting console mode...");
    // Aici adaugi logica de inițializare consolă dacă e cazul
}

// 6. Metoda run() - bucla principală a aplicației
int App::run() {
    if (!init()) {
        LOG_FATAL(L"[APP] Initialization failed!");
        return 1;
    }

    LOG_SUCCESS(L"[APP] Application is running...");

    // În modul SERVICE/CONSOLE, de regulă se aşteaptă un semnal sau se rulează bucla
    return 0;
}

// =========================================================================
// GESTIUNE CONEXIUNI BAZĂ DE DATE
// =========================================================================

bool App::addConnection(const std::string& name, std::unique_ptr<dbConnection> conn) {
    if (!conn) {
        LOG_ERROR(L"App::addConnection: S-a încercat adăugarea unei conexiuni nule.");
        return false;
    }

    if (m_connections.find(name) != m_connections.end()) {
        LOG_WARNING(L"App::addConnection: Suprascriere conexiune existentă -> " + std::wstring(name.begin(), name.end()));
        removeConnection(name);
    }

    m_connections[name] = std::move(conn);
    LOG_INFO(L"App::addConnection: Conexiune adăugată cu succes -> " + std::wstring(name.begin(), name.end()));
    return true;
}

dbConnection* App::getConnection(const std::string& name) const {
    auto it = m_connections.find(name);
    if (it != m_connections.end()) {
        return it->second.get();
    }
    LOG_ERROR(L"App::getConnection: Conexiunea nu a fost găsită -> " + std::wstring(name.begin(), name.end()));
    return nullptr;
}

bool App::hasConnection(const std::string& name) const {
    return m_connections.find(name) != m_connections.end();
}

void App::removeConnection(const std::string& name) {
    auto it = m_connections.find(name);
    if (it != m_connections.end()) {
        if (it->second) {
            it->second->closeDatabase();
        }
        m_connections.erase(it);
        LOG_INFO(L"App::removeConnection: Conexiune eliminată -> " + std::wstring(name.begin(), name.end()));
    }
}

void App::closeAllConnections() {
    for (auto& pair : m_connections) {
        if (pair.second) {
            pair.second->closeDatabase();
        }
    }
    m_connections.clear();
    LOG_INFO(L"App::closeAllConnections: Toate conexiunile au fost închise.");
}

void App::shutdown() {
    closeAllConnections();
}