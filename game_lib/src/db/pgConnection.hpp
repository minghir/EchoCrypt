#ifndef PGCONNECTION_HPP
#define PGCONNECTION_HPP

#include "dbConnection.hpp"
#include <libpq-fe.h>
#include <string>
#include <vector>
#include <map>

class pgConnection : public dbConnection {
private:
    std::string type;          // "pgsql"
    std::wstring dsn;          // Connection string (Unicode)
    PGconn* conn;              // Handler conexiune PostgreSQL
    std::wstring error;

    // Stocare rezultate și starea iterării per statement
    std::map<std::string, PGresult*> results;
    std::map<std::string, int> currentRows;      // Indexul rândului curent (-1 = înainte de primul fetch)
    std::map<std::string, int> totalRows;        // Numărul total de rânduri returnate

    // Cache pentru metadate per statement
    std::map<std::string, std::vector<std::wstring>> colNames;
    std::map<std::string, std::map<std::wstring, int>> colNameIndexes;

    // Helper-e interne pentru conversie UTF-8 / UTF-16
    std::string wstringToUtf8(const std::wstring& wstr) const;
    std::wstring utf8ToWstring(const std::string& str) const;

    // Mapare tipuri OID PostgreSQL -> vNativeDataType
    vNativeDataType mapPgOidToUniversal(Oid oid) const;

    vConResult m_lastResult;

public:
    pgConnection(const std::string& type, const std::wstring& dsn);
    pgConnection();
    ~pgConnection() override;

    bool openDatabase() override;
    void closeDatabase() override;
    bool isConnected() const override;

    bool reconnect() override;
    bool testConnection() override;

    bool execQuery(const std::wstring& query, std::string stm_name = "default") override;
    bool execQuery(const std::wstring& query, const std::vector<std::wstring>& params, std::string stm_name = "default") override;

    long long execCountQuery(const std::wstring& countQuery) override;

    int getRowCount(std::string stm_name = "default") override;

    const std::vector<std::wstring>& getColumnNames(std::string stm_name = "default") override;
    const std::vector<vNativeDataType> getColumnTypes(std::string stm_name = "default") override;
    const std::vector<vExternalColumnInfo> getColumnsInfo(std::string stm_name = "default") override;

    std::wstring fetchFieldByNumber(int fieldNo, std::string stm_name = "default") override;
    bool fetchNextRow(std::string stm_name = "default") override;
    std::vector<std::wstring> fetchRow(std::string stm_name = "default") override;
    std::wstring fetchFieldByName(const std::wstring& fieldName, std::string stm_name = "default") override;
    std::map<std::wstring, std::wstring> fetchMap(std::string stm_name = "default") override;

    std::wstring getError() override;
    void clearError() override;

    std::string getConnectionType() override;
    std::wstring getConnectionDSN() override;
    void setConnectionDSN(const std::wstring& txt) override;

    vConResult getLastQueryResult() override { return m_lastResult; }

    std::vector<vExternalColumnInfo> getTableSchema(const std::wstring& tableName) override;
    void clearStatement(std::string stm_name = "default") override;
};

#endif // PGCONNECTION_HPP