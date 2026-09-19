#include "pgConnection.hpp"
#include "../ConsoleManager.hpp"
#include <codecvt>
#include <locale>
#include <sstream>

// ============================================================================
// CONVERTIROARE UTF-8 <-> UTF-16
// ============================================================================
#include <windows.h>

std::string pgConnection::wstringToUtf8(const std::wstring& wstr) const {
    if (wstr.empty()) return "";

    int sizeNeeded = WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        NULL,
        0,
        NULL,
        NULL
    );

    if (sizeNeeded <= 0) return "";

    std::string strTo(sizeNeeded, 0);
    WideCharToMultiByte(
        CP_UTF8,
        0,
        wstr.c_str(),
        static_cast<int>(wstr.length()),
        &strTo[0],
        sizeNeeded,
        NULL,
        NULL
    );

    return strTo;
}

std::wstring pgConnection::utf8ToWstring(const std::string& str) const {
    if (str.empty()) return L"";

    int sizeNeeded = MultiByteToWideChar(
        CP_UTF8,
        0,
        str.c_str(),
        static_cast<int>(str.length()),
        NULL,
        0
    );

    if (sizeNeeded <= 0) return L"";

    std::wstring wstrTo(sizeNeeded, 0);
    MultiByteToWideChar(
        CP_UTF8,
        0,
        str.c_str(),
        static_cast<int>(str.length()),
        &wstrTo[0],
        sizeNeeded
    );

    return wstrTo;
}

// ============================================================================
// CONSTRUCTORI & DESTRUCTOR
// ============================================================================
pgConnection::pgConnection(const std::string& type, const std::wstring& dsn)
    : type(type), dsn(dsn), conn(nullptr) {
}

pgConnection::pgConnection()
    : type("pgsql"), dsn(L""), conn(nullptr) {
}

pgConnection::~pgConnection() {
    closeDatabase();
}

// ============================================================================
// GESTIUNE CONEXIUNE
// ============================================================================
bool pgConnection::openDatabase() {
    closeDatabase(); // Ne asigurăm că eliberăm resursele anterioare

    std::string conninfo = wstringToUtf8(dsn);
    conn = PQconnectdb(conninfo.c_str());

    if (PQstatus(conn) != CONNECTION_OK) {
        error = utf8ToWstring(PQerrorMessage(conn));
        LOG_ERROR(L"pgConnection::openDatabase error: " + error);
        PQfinish(conn);
        conn = nullptr;
        return false;
    }

    // Setăm client encoding pe UTF8
    PQsetClientEncoding(conn, "UTF8");
    LOG_SUCCESS(L"pgConnection::openDatabase: Conectat cu succes la PostgreSQL.");
    return true;
}

void pgConnection::closeDatabase() {
    // Eliberăm toate rezultatele PGresult stocate
    for (auto& pair : results) {
        if (pair.second) {
            PQclear(pair.second);
        }
    }
    results.clear();
    currentRows.clear();
    totalRows.clear();
    colNames.clear();
    colNameIndexes.clear();

    if (conn) {
        PQfinish(conn);
        conn = nullptr;
    }
}

bool pgConnection::isConnected() const {
    return (conn != nullptr && PQstatus(conn) == CONNECTION_OK);
}

bool pgConnection::testConnection() {
    if (!isConnected()) return false;
    PGresult* res = PQexec(conn, "SELECT 1");
    bool alive = (PQresultStatus(res) == PGRES_TUPLES_OK);
    PQclear(res);
    return alive;
}

bool pgConnection::reconnect() {
    if (testConnection()) return true;
    closeDatabase();
    return openDatabase();
}

// ============================================================================
// EXECUȚIE QUERY
// ============================================================================

bool pgConnection::execQuery(const std::wstring& query, std::string stm_name) {
    clearError();
    if (!isConnected()) {
        if (!reconnect()) return false;
    }

    // Curățăm rezultatul anterior pentru acest stm_name
    clearStatement(stm_name);

    std::string utf8Query = wstringToUtf8(query);
    PGresult* res = PQexec(conn, utf8Query.c_str());

    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        error = utf8ToWstring(PQerrorMessage(conn));
        LOG_ERROR(L"pgConnection::execQuery error: " + error);
        PQclear(res);
        return false;
    }

    // Salvăm noul rezultat
    results[stm_name] = res;
    currentRows[stm_name] = -1; // resetăm poziția cursorului

    int nTuples = PQntuples(res);
    totalRows[stm_name] = nTuples;

    // Cache-uim numele coloanelor
    int nCols = PQnfields(res);
    for (int i = 0; i < nCols; ++i) {
        std::wstring colName = utf8ToWstring(PQfname(res, i));
        colNames[stm_name].push_back(colName);
        colNameIndexes[stm_name][colName] = i + 1; // 1-based indexing
    }

    return true;
}

bool pgConnection::execQuery(const std::wstring& query, const std::vector<std::wstring>& params, std::string stm_name) {
    clearError();
    if (!isConnected()) {
        if (!reconnect()) return false;
    }

    clearStatement(stm_name);

    std::string utf8Query = wstringToUtf8(query);
    int nParams = static_cast<int>(params.size());

    std::vector<std::string> utf8Params;
    std::vector<const char*> paramValues;

    utf8Params.reserve(nParams);
    paramValues.reserve(nParams);

    for (const auto& p : params) {
        utf8Params.push_back(wstringToUtf8(p));
    }
    for (int i = 0; i < nParams; ++i) {
        paramValues.push_back(utf8Params[i].c_str());
    }

    PGresult* res = PQexecParams(
        conn,
        utf8Query.c_str(),
        nParams,
        NULL, // Lasă Postgres să deducă tipurile
        paramValues.data(),
        NULL, // Lungimile parametrilor (text)
        NULL, // Formatele parametrilor (0 = text)
        0     // Format rezultat (0 = text)
    );

    ExecStatusType status = PQresultStatus(res);

    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        error = utf8ToWstring(PQerrorMessage(conn));
        LOG_ERROR(L"pgConnection::execQuery (Params) error: " + error);
        PQclear(res);
        return false;
    }

    results[stm_name] = res;
    currentRows[stm_name] = -1;
    totalRows[stm_name] = PQntuples(res);

    int nCols = PQnfields(res);
    for (int i = 0; i < nCols; ++i) {
        std::wstring colName = utf8ToWstring(PQfname(res, i));
        colNames[stm_name].push_back(colName);
        colNameIndexes[stm_name][colName] = i + 1;
    }

    return true;
}

long long pgConnection::execCountQuery(const std::wstring& countQuery) {
    if (!execQuery(countQuery, "count_stmt")) {
        return -1;
    }
    if (fetchNextRow("count_stmt")) {
        std::wstring val = fetchFieldByNumber(1, "count_stmt");
        clearStatement("count_stmt");
        try {
            return std::stoll(val);
        }
        catch (...) {
            return -1;
        }
    }
    clearStatement("count_stmt");
    return -1;
}

// ============================================================================
// FETCHING DATA
// ============================================================================
bool pgConnection::fetchNextRow(std::string stm_name) {
    auto it = results.find(stm_name);
    if (it == results.end() || !it->second) return false;

    int nextRow = currentRows[stm_name] + 1;
    if (nextRow < totalRows[stm_name]) {
        currentRows[stm_name] = nextRow;
        return true;
    }
    return false;
}

std::wstring pgConnection::fetchFieldByNumber(int fieldNo, std::string stm_name) {
    auto it = results.find(stm_name);
    if (it == results.end() || !it->second) return L"";

    int currRow = currentRows[stm_name];
    if (currRow < 0 || currRow >= totalRows[stm_name]) return L"";

    int colIndex = fieldNo - 1; // Conversie de la 1-based la 0-based
    if (colIndex < 0 || colIndex >= PQnfields(it->second)) return L"";

    if (PQgetisnull(it->second, currRow, colIndex)) {
        return L"";
    }

    char* val = PQgetvalue(it->second, currRow, colIndex);
    return utf8ToWstring(val);
}

std::vector<std::wstring> pgConnection::fetchRow(std::string stm_name) {
    std::vector<std::wstring> row;
    auto it = results.find(stm_name);
    if (it == results.end() || !it->second) return row;

    int nCols = PQnfields(it->second);
    for (int i = 1; i <= nCols; ++i) {
        row.push_back(fetchFieldByNumber(i, stm_name));
    }
    return row;
}

std::wstring pgConnection::fetchFieldByName(const std::wstring& fieldName, std::string stm_name) {
    auto it = colNameIndexes.find(stm_name);
    if (it == colNameIndexes.end()) return L"";

    auto idxIt = it->second.find(fieldName);
    if (idxIt == it->second.end()) return L"";

    return fetchFieldByNumber(idxIt->second, stm_name);
}

std::map<std::wstring, std::wstring> pgConnection::fetchMap(std::string stm_name) {
    std::map<std::wstring, std::wstring> rowMap;
    auto it = results.find(stm_name);
    if (it == results.end() || !it->second) return rowMap;

    int nCols = PQnfields(it->second);
    for (int i = 1; i <= nCols; ++i) {
        std::wstring cName = colNames[stm_name][i - 1];
        rowMap[cName] = fetchFieldByNumber(i, stm_name);
    }
    return rowMap;
}

int pgConnection::getRowCount(std::string stm_name) {
    auto it = totalRows.find(stm_name);
    return (it != totalRows.end()) ? it->second : 0;
}

// ============================================================================
// METADATE & TIPURI DE DATE
// ============================================================================
vNativeDataType pgConnection::mapPgOidToUniversal(Oid oid) const {
    switch (oid) {
    case 20: // INT8 / BIGINT
        return vNativeDataType::V_BIGINT;
    case 21: // INT2 / SMALLINT
    case 23: // INT4 / INTEGER
        return vNativeDataType::V_INTEGER;
    case 700: // FLOAT4 / REAL
    case 701: // FLOAT8 / DOUBLE PRECISION
    case 1700: // NUMERIC / DECIMAL
        return vNativeDataType::V_DOUBLE;
    case 1082: // DATE
    case 1114: // TIMESTAMP
    case 1184: // TIMESTAMPTZ
        return vNativeDataType::V_DATE;
    case 16: // BOOL
        return vNativeDataType::V_BOOLEAN;
    case 17: // BYTEA
        return vNativeDataType::V_BLOB;
    default: // TEXT, VARCHAR, CHAR etc.
        return vNativeDataType::V_TEXT;
    }
}

const std::vector<std::wstring>& pgConnection::getColumnNames(std::string stm_name) {
    return colNames[stm_name];
}

const std::vector<vNativeDataType> pgConnection::getColumnTypes(std::string stm_name) {
    std::vector<vNativeDataType> types;
    auto it = results.find(stm_name);
    if (it != results.end() && it->second) {
        int nCols = PQnfields(it->second);
        for (int i = 0; i < nCols; ++i) {
            Oid oid = PQftype(it->second, i);
            types.push_back(mapPgOidToUniversal(oid));
        }
    }
    return types;
}

const std::vector<vExternalColumnInfo> pgConnection::getColumnsInfo(std::string stm_name) {
    std::vector<vExternalColumnInfo> infoList;
    auto it = results.find(stm_name);
    if (it == results.end() || !it->second) return infoList;

    int nCols = PQnfields(it->second);
    for (int i = 0; i < nCols; ++i) {
        vExternalColumnInfo info;
        info.name = utf8ToWstring(PQfname(it->second, i));
        Oid oid = PQftype(it->second, i);
        info.type = mapPgOidToUniversal(oid);
        info.length = PQfsize(it->second, i);
        info.precision = 0; // Se poate obține din mod (PQfmod) dacă e necesar
        info.isNullable = true;
        infoList.push_back(info);
    }
    return infoList;
}

std::vector<vExternalColumnInfo> pgConnection::getTableSchema(const std::wstring& tableName) {
    std::vector<vExternalColumnInfo> schema;

    std::wstring schemaName = L"public";
    std::wstring pureTableName = tableName;

    size_t dotPos = tableName.find(L'.');
    if (dotPos != std::wstring::npos) {
        schemaName = tableName.substr(0, dotPos);
        pureTableName = tableName.substr(dotPos + 1);
    }

    std::wstring schemaQuery =
        L"SELECT column_name, data_type, character_maximum_length, is_nullable "
        L"FROM information_schema.columns "
        L"WHERE table_schema = '" + schemaName + L"' AND table_name = '" + pureTableName + L"' "
        L"ORDER BY ordinal_position;";

    if (execQuery(schemaQuery, "schema_stmt")) {
        while (fetchNextRow("schema_stmt")) {
            vExternalColumnInfo info;
            info.name = fetchFieldByName(L"column_name", "schema_stmt");

            std::wstring dType = fetchFieldByName(L"data_type", "schema_stmt");
            if (dType == L"integer" || dType == L"smallint") info.type = vNativeDataType::V_INTEGER;
            else if (dType == L"bigint") info.type = vNativeDataType::V_BIGINT;
            else if (dType == L"numeric" || dType == L"real" || dType == L"double precision") info.type = vNativeDataType::V_DOUBLE;
            else if (dType == L"boolean") info.type = vNativeDataType::V_BOOLEAN;
            else if (dType.find(L"date") != std::wstring::npos || dType.find(L"timestamp") != std::wstring::npos) info.type = vNativeDataType::V_DATE;
            else info.type = vNativeDataType::V_TEXT;

            std::wstring lenStr = fetchFieldByName(L"character_maximum_length", "schema_stmt");
            info.length = lenStr.empty() ? 0 : std::stoi(lenStr);
            info.isNullable = (fetchFieldByName(L"is_nullable", "schema_stmt") == L"YES");

            schema.push_back(info);
        }
        clearStatement("schema_stmt");
    }

    return schema;
}

// ============================================================================
// METODE AUXILIARE
// ============================================================================
std::wstring pgConnection::getError() {
    return error;
}

void pgConnection::clearError() {
    error.clear();
}

std::string pgConnection::getConnectionType() {
    return type;
}

std::wstring pgConnection::getConnectionDSN() {
    return dsn;
}

void pgConnection::setConnectionDSN(const std::wstring& txt) {
    dsn = txt;
}

void pgConnection::clearStatement(std::string stm_name) {
    auto it = results.find(stm_name);
    if (it != results.end()) {
        if (it->second) {
            PQclear(it->second);
        }
        results.erase(it);
    }
    currentRows.erase(stm_name);
    totalRows.erase(stm_name);
    colNames.erase(stm_name);
    colNameIndexes.erase(stm_name);
}