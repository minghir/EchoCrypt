#ifndef APP_HPP
#define APP_HPP

#include "ConsoleManager.hpp" 
#include "db/dbConnection.hpp"

#include <string>
#include <memory>
#include <unordered_map>

enum class RunMode { CONSOLE, SERVICE };

class App {
public:
    explicit App(RunMode mode = RunMode::CONSOLE);
    virtual ~App(); // Modificat: curățăm conexiunile în destructor

    int run();
    bool init();
    virtual bool initConsole() { return true; }
    virtual bool initService() { return true; }
    static App* getAppInstance() { return s_instance; }

    void shutdown();
    void startConsole();

    // =========================================================================
    // GESTIUNE CONEXIUNI BAZĂ DE DATE
    // =========================================================================

    /**
     * @brief Înregistrează o conexiune existentă (mută ownership-ul către App).
     * @param name Numele de identificare (ex: "default", "pg_main")
     * @param conn Pointer unic către obiectul derivat din dbConnection
     */
    bool addConnection(const std::string& name, std::unique_ptr<dbConnection> conn);

    /**
     * @brief Preia un pointer brut la o conexiune înregistrată.
     * @param name Numele conexiunii
     * @return dbConnection* Pointer la conexiune sau nullptr dacă nu există
     */
    dbConnection* getConnection(const std::string& name = "default") const;

    /**
     * @brief Verifică dacă o conexiune există în registry.
     */
    bool hasConnection(const std::string& name) const;

    /**
     * @brief Închide și elimină o conexiune specifică din rețea.
     */
    void removeConnection(const std::string& name);

    /**
     * @brief Închide și eliberează toate conexiunile active.
     */
    void closeAllConnections();

protected:
    void setRunMode(RunMode mode) { m_runMode = mode; }

private:
    static App* s_instance;
    RunMode m_runMode = RunMode::CONSOLE;

    // Stocarea conexiunilor cu ownership exclusiv
    std::unordered_map<std::string, std::unique_ptr<dbConnection>> m_connections;
};

#endif // APP_HPP