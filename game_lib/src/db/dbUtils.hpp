#ifndef DBUTILS_HPP
#define DBUTILS_HPP
#include "dbConnection.hpp"

#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <sstream>

/**
 * @brief Extrage o singură valoare dintr-un câmp specific al unei tabele, pe baza unei chei primare.
 * * @param db        Pointer către conexiunea activă de bază de date (trebuie să fie deschisă).
 * @param fieldPath Calea completă către câmp, în formatul "schema.tabel.coloana" sau "tabel.coloana".
 * @param pkValue   Valoarea cheii primare pentru filtrare (ex: "1001").
 * @param pkColumn  Numele coloanei care servește drept cheie primară. Implicit este "ID".
 * * @return std::wstring Valoarea extrasă din baza de date sau un șir gol/mesaj de eroare dacă:
 * - Conexiunea este invalidă.
 * - Calea fieldPath nu este bine formatată.
 * - Înregistrarea nu a fost găsită.
 * * @note Funcția utilizează un statement name intern (internal_lookup_...) pentru a nu interfera
 * cu alte cursoare de date (SELECT-uri) aflate în derulare pe statement-ul "default".
 */

std::wstring getDbValueFromField(dbConnection* db,
    const std::wstring& fieldPath,
    const std::wstring& pkValue,
    const std::wstring& pkColumn = L"ID");


/**
 * @brief Execută un query SQL personalizat și returnează valoarea dintr-o coloană specifică a primului rând rezultat.
 * * @param db       Pointer către conexiunea activă de bază de date.
 * @param query    Instrucțiunea SQL completă (ex: "SELECT nume FROM utilizatori WHERE email = 'test@mail.ro'").
 * @param column   Numele coloanei a cărei valoare trebuie returnată.
 * * @return std::wstring Valoarea câmpului cerut din primul rând returnat, sau un șir gol dacă:
 * - Query-ul nu returnează niciun rând.
 * - Coloana specificată nu există în setul de rezultate.
 * - A apărut o eroare la execuția SQL.
 * * @note Util pentru lookup-uri rapide unde logica de filtrare este mai complexă decât o simplă cheie primară.
 * Folosește un statement name izolat pentru a nu corupe cursoarele externe.
 */
std::wstring getDbValueFromQuery(dbConnection* db,
    const std::wstring& query,
    const std::wstring& column);

std::map<std::wstring, std::wstring> getDbRowMapFromQuery(
    dbConnection* db,
    const std::wstring& query);

#endif