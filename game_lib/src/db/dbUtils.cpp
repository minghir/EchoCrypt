#include "dbUtils.hpp"
#include "../ConsoleManager.hpp"


std::wstring getDbValueFromField(dbConnection* db,
    const std::wstring& fieldPath,
    const std::wstring& pkValue,
    const std::wstring& pkColumn)
{
    if (!db || !db->isConnected()) {
        return L"[ERROR] No database connection";
    }

    // 1. Parsăm fieldPath
    std::vector<std::wstring> parts;
    std::wstringstream ss(fieldPath);
    std::wstring segment;
    while (std::getline(ss, segment, L'.')) {
        parts.push_back(segment);
    }

    if (parts.size() < 2) {
        return L"[ERROR] Field path must be at least 'table.column'";
    }

    std::wstring columnName = parts.back();
    parts.pop_back();

    std::wstring tableName;
    for (size_t i = 0; i < parts.size(); ++i) {
        tableName += parts[i];
        if (i < parts.size() - 1) tableName += L".";
    }

    // 2. Construim Query-ul
    std::wstring query = L"SELECT " + columnName + L" FROM " + tableName +
        L" WHERE " + pkColumn + L" = '" + pkValue + L"'";

    // 3. Executăm pe un statement intern
    std::string internalStm = "internal_lookup_" + std::to_string(rand() % 1000);
    std::wstring result = L"";

    if (db->execQuery(query, internalStm)) {
        if (db->fetchNextRow(internalStm)) {
            result = db->fetchFieldByName(columnName, internalStm);
        }
    }
    else {
        LOG_ERROR(L"[getValueFromField] query: " + query);
    }

    // --- CURĂȚENIE: Eliberăm resursele statement-ului înainte de return ---
    db->clearStatement(internalStm);

    return result;
}

std::wstring getDbValueFromQuery(dbConnection* db,
    const std::wstring& query,
    const std::wstring& column)
{
    if (!db || !db->isConnected()) {
        return L"";
    }

    std::string internalStm = "query_lookup_" + std::to_string(rand() % 1000);
    std::wstring result = L"";

    if (db->execQuery(query, internalStm)) {
        if (db->fetchNextRow(internalStm)) {
            result = db->fetchFieldByName(column, internalStm);
        }
        else {
            //vMessageDialog::Warning(L"Aici1:" + query);
        }
    }
    else {
        std::wstring err_msg = L"[getDbValueFromQuery] query: " + query;
        LOG_ERROR(err_msg);
        //vMessageDialog::Error(err_msg);
    }

    // --- CURĂȚENIE: Eliberăm resursele statement-ului înainte de return ---
    db->clearStatement(internalStm);
   //vMessageDialog::Warning(L"Aici:" + result);
    return result;
}

std::map<std::wstring, std::wstring> getDbRowMapFromQuery(
    dbConnection* db,
    const std::wstring& query)
{
    std::map<std::wstring, std::wstring> result;

    if (!db || !db->isConnected()) {
        return result;
    }

    std::string internalStm = "row_map_lookup_" + std::to_string(rand() % 1000);

    if (db->execQuery(query, internalStm)) {
        if (db->fetchNextRow(internalStm)) {
            // Folosim getColumnNames care este membru în dbConnection
            const auto& colNames = db->getColumnNames(internalStm);

            for (const auto& colName : colNames) {
                result[colName] = db->fetchFieldByName(colName, internalStm);
            }
        }
    }
    else {
        std::wstring err_msg = L"[getDbRowMapFromQuery] query: " + query;
        LOG_ERROR(err_msg);
    }

    // Curățăm resursele statement-ului
    db->clearStatement(internalStm);

    return result;
}