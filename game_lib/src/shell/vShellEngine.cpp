#include <variant>

#include <filesystem>
#include <fstream>

#include "vShellEngine.hpp"
#include "../ConsoleManager.hpp"
#include "../stringUtils.hpp"
#include "../ConfigLoader.hpp"


vShellEngine::vShellEngine() : m_running(true) {
    initializeCommandsHandlers();
    initializeFunctionsHandlers();


    std::wstring dbfPath, csvPath;

    ConfigLoader cfg;
    if (cfg.load("ImportTaxare.cfg")) {
        dbfPath = cfg.get("Dbf_file_path", L"./import/dbf/");
        csvPath = cfg.get("Csv_file_path", L"./import/dbf/");
        //LOG_INFO(L"Config loaded. DBF Path: " + dbfPath);
        //LOG_INFO(L"Config loaded. DBF Path: " + csvPath);
    }
    else {
        LOG_ERROR(L"Configuration file not found!");
    }
}




void vShellEngine::execute(const std::wstring& line) {
    std::wstring cleanLine = normalizeSpaces(line);
    if (cleanLine.empty() && m_accumulator.empty()) return;
    
    // 1. Verificăm dacă linia curentă cere continuare
    bool continues = false;
    if (!cleanLine.empty() && cleanLine.back() == L'\\') {
        continues = true;
        cleanLine.pop_back(); // Eliminăm '\'
    }

    // 2. Adăugăm bucata curentă la acumulator
    // Punem un spațiu doar dacă nu e prima bucată
    if (!m_accumulator.empty() && !cleanLine.empty()) m_accumulator += L" ";
    m_accumulator += cleanLine;

    // 3. Dacă linia se termină în '\', ieșim și așteptăm următoarea intrare
    if (continues) {
        // Opțional: poți schimba prompt-ul consolei aici în ">> " pentru feedback vizual
        return;
    }

    // 4. Acum avem linia completă în m_accumulator. Decidem ce este:
    std::wstring fullCommand = normalizeSpaces(m_accumulator);
    m_accumulator.clear(); // Resetăm pentru următoarea utilizare

    if (fullCommand.empty()) return;

    // CAZ A: Comandă Shell
    if (fullCommand[0] == L'/') {
        if (!fullCommand.empty() && fullCommand.back() == L';') {
            fullCommand.pop_back();
        }

        // dacă linia conține && sau ||, o tratăm ca UN SINGUR lanț logic
        if (fullCommand.find(L"&&") != std::wstring::npos ||
            fullCommand.find(L"||") != std::wstring::npos) {

            executeLogicalLine(fullCommand);
        }
        else {
            // altfel, putem avea mai multe comenzi pe linie, separate prin '/'
            auto commands = splitShellCommands(fullCommand);
            for (auto& cmd : commands) {
                executeShellCommand(cmd);
            }
        }
    }


    // CAZ B: SQL Query
    else {
        // Pentru SQL, verificăm dacă avem ';' obligatoriu
        if (fullCommand.find(L';') != std::wstring::npos) {
            processQuery(fullCommand);
        }
        else {
            // Dacă nu are ';' și nu are '\', o păstrăm în acumulator 
            // sau dăm eroare, depinde de preferința ta.
            m_accumulator = fullCommand;
        }
    }
}

void print_pretty_table(const std::vector<std::wstring>& headers,
    const std::vector<std::vector<std::wstring>>& rows) {
    if (headers.empty()) return;

    // 1. Calculăm lățimea maximă pentru fiecare coloană
    std::vector<size_t> colWidths(headers.size());
    for (size_t i = 0; i < headers.size(); ++i) {
        colWidths[i] = headers[i].length();
        for (const auto& row : rows) {
            if (i < row.size())
                colWidths[i] = (std::max)(colWidths[i], row[i].length());
        }
        colWidths[i] += 2; // Padding
    }

    // 2. Funcție pentru linii orizontale
    auto print_line = [&]() {
        std::wcout << L"+";
        for (size_t width : colWidths) {
            std::wcout << std::wstring(width, L'-') << L"+";
        }
        std::wcout << L"\n";
    };

    // 3. Afișăm Antetul
    print_line();
    std::wcout << L"|";
    for (size_t i = 0; i < headers.size(); ++i) {
        std::wcout << L" " << std::left << std::setw(colWidths[i] - 1) << headers[i] << L"|";
    }
    std::wcout << L"\n";
    print_line();

    // 4. Afișăm Rândurile
    for (const auto& row : rows) {
        std::wcout << L"|";
        for (size_t i = 0; i < row.size(); ++i) {
            std::wcout << L" " << std::left << std::setw(colWidths[i] - 1) << row[i] << L"|";
        }
        std::wcout << L"\n";
    }
    print_line();
    std::wcout << L"(" << rows.size() << L" rows in set)\n\n";
}



void vShellEngine::processQuery(std::wstring query) {
    if (dbfConn->execQuery(query)) {
        LOG_SUCCESS(L"Query executat cu succes.");

        auto headers = dbfConn->getColumnNames();
        std::vector<std::vector<std::wstring>> data;

        // Colectăm toate rândurile pentru a putea calcula lățimea coloanelor
        while (dbfConn->fetchNextRow()) {
            data.push_back(dbfConn->fetchRow()); // Presupunem că fetchRow returnează vector de wstring
        }

        if (data.empty()) {
            //LOG_INFO(L"Empty set (0 rows).");
        }
        else {
            print_pretty_table(headers, data);
        }
    }
    else {
        LOG_ERROR(L"Eroare SQL: " + dbfConn->getError());
    }
}

std::vector<std::wstring> vShellEngine::splitShellCommands(const std::wstring& line) {
    std::vector<std::wstring> cmds;
    std::wstring current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t c = line[i];

        if (c == L'"')
            inQuotes = !inQuotes;

        if (!inQuotes && c == L'/' && !current.empty()) {

            // eliminăm spațiile de la final
            size_t j = current.size();
            while (j > 0 && iswspace(current[j - 1])) {
                j--;
            }

            bool isLogical = false;

            if (j >= 2) {
                std::wstring prev2 = current.substr(j - 2, 2);
                if (prev2 == L"&&" || prev2 == L"||") {
                    isLogical = true;
                }
            }

            if (!isLogical) {
                cmds.push_back(normalizeSpaces(current));
                current.clear();
            }
        }



        current += c;
    }

    if (!current.empty())
        cmds.push_back(normalizeSpaces(current));

    return cmds;
}




bool vShellEngine::executeShellCommand(const std::wstring& line) {
    ShellCommand sc = vShellEngineCommandParser::parse(line);
    if (!sc.isValid) return false;

    // Dispecer de procesare a argumentelor
    if (sc.name == L"/s" || sc.name == L"/set") {
        sc = processSetArgs(sc);
    }
    else if (sc.name == L"/eval") {
        // Aici poți adăuga processEvalArgs(sc) pe viitor
        for (auto& arg : sc.args) arg = processArgument(arg);
    }
    else {
        // Ramura default: procesăm totul normal (comenzi simple ca /echo)
        for (auto& arg : sc.args) {
            arg = processArgument(arg);
        }
    }

    // Execuția propriu-zisă
    auto it = m_commandHandlers.find(sc.name);
    if (it != m_commandHandlers.end()) {
        return it->second(sc);
    }
    else {
        
        LOG_ERROR(L"Unknown command: " + sc.name);
        return false;
    }
}

std::vector<CommandBlock> vShellEngine::splitByLogicalOperators(const std::wstring& line) {
    std::vector<CommandBlock> blocks;
    std::wstring current;
    bool inQuotes = false;

    for (size_t i = 0; i < line.size(); ++i) {
        wchar_t c = line[i];

        if (c == L'"')
            inQuotes = !inQuotes;

        if (!inQuotes && i + 1 < line.size()) {
            std::wstring two = line.substr(i, 2);

            if (two == L"&&" || two == L"||") {
                blocks.push_back({ normalizeSpaces(current), two });
                current.clear();
                i++; // skip operator
                continue;
            }
        }

        current += c;
    }

    if (!current.empty())
        blocks.push_back({ normalizeSpaces(current), L"" });

    return blocks;
}

bool vShellEngine::executeLogicalLine(const std::wstring& line) {
    auto blocks = splitByLogicalOperators(line);

    bool last = true;

    for (auto& b : blocks) {
        if (b.op == L"&&" && !last)
            continue;

        if (b.op == L"||" && last)
            continue;

        last = executeShellCommand(b.cmd);
    }

    return last;
}


std::wstring vShellEngine::vValueToString(const vDataValue& val) {
//std::wstring vShellEngine::vValueToString(const vValue& val) {
    return std::visit([](auto&& arg) -> std::wstring {
        using T = std::decay_t<decltype(arg)>;

        // 1. Gestionare NULL (monostate)
        // Returnăm un string gol sau "NULL" pentru a nu strica query-urile viitoare
        if constexpr (std::is_same_v<T, std::monostate>) {
            return L"";
        }
        // 2. String-uri (wstring)
        else if constexpr (std::is_same_v<T, std::wstring>) {
            return arg;
        }
        // 3. Numere întregi (int, long long, etc.)
        else if constexpr (std::is_integral_v<T> && !std::is_same_v<T, bool>) {
            return std::to_wstring(arg);
        }
        // 4. Numere cu virgulă (double, float)
        else if constexpr (std::is_floating_point_v<T>) {
            std::wstringstream ss;
            // setprecision(2) e bun pentru afișaj, dar dacă vrei precizie totală 
            // poți scoate std::fixed
            ss << std::fixed << std::setprecision(2) << arg;
            std::wstring s = ss.str();

            // Eliminăm zerourile inutile de la final (opțional)
            if (s.find(L'.') != std::wstring::npos) {
                s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
                if (s.back() == L'.') s.pop_back();
            }
            return s;
        }
        // 5. Boolean
        else if constexpr (std::is_same_v<T, bool>) {
            return arg ? L"1" : L"0"; // Returnăm 1/0 pentru compatibilitate SQL mai bună
        }
        // 6. Fallback
        else {
            return L"";
        }
        }, val);
}


/*
void vShellEngine::executeShellCommand(const std::wstring& line) {
    ShellCommand sc = vShellEngineCommandParser::parse(line);
    if (!sc.isValid) return;

    // REZOLVARE DINAMICĂ: Transformăm FACT(3) în 6 înainte de a ajunge la Handler
    for (auto& arg : sc.args) {
        LOG_WARNING(L"{" + arg + L"}");
        arg = processArgument(arg);
    }

    auto it = m_commandHandlers.find(sc.name);
    if (it != m_commandHandlers.end()) {
        it->second(sc); // Acum /echo va primi direct "6" în args[0]
    }
    else {
        LOG_ERROR(L"Unknown command: " + sc.name);
    }
    }
    */




ShellCommand vShellEngine::processSetArgs(ShellCommand sc) {
    /*
    bool foundEquals = false;

    for (auto& arg : sc.args) {
        if (foundEquals) {
            // Suntem după egal: aici procesăm tot (variabile, funcții)
            arg = processArgument(arg);
        }
        else {
            size_t eqPos = arg.find(L'=');
            if (eqPos != std::wstring::npos) {
                // Am găsit argumentul care conține '=' (ex: "$a=10" sau "=")
                std::wstring left = arg.substr(0, eqPos + 1); // "$a="
                std::wstring right = arg.substr(eqPos + 1);   // "10"

                if (!right.empty()) {
                    right = processArgument(right);
                }
                arg = left + right;
                foundEquals = true;
            }
            else {
                // Suntem înainte de egal (numele variabilei): NU procesăm nimic
                // Rămâne crud, ex: $a[1]
            }
        }
    }
    */
    return sc;
}


std::vector<std::wstring> vShellEngine::splitArguments(const std::wstring& s) {
    std::vector<std::wstring> args;
    std::wstring current;
    int depth = 0;
    bool inQuotes = false;

    for (wchar_t c : s) {
        if (c == L'"') inQuotes = !inQuotes;
        if (!inQuotes) {
            if (c == L'(' || c == L'[') depth++;
            if (c == L')' || c == L']') depth--;
            if (c == L',' && depth == 0) {
                // TRIM doar la spațiile exterioare înainte de a adăuga în listă
                size_t first = current.find_first_not_of(L" ");
                if (first != std::wstring::npos) {
                    size_t last = current.find_last_not_of(L" ");
                    args.push_back(current.substr(first, (last - first + 1)));
                }
                else {
                    args.push_back(L"");
                }
                current.clear();
                continue;
            }
        }
        current += c;
    }
    // Trim și pentru ultimul argument
    if (!current.empty()) {
        size_t first = current.find_first_not_of(L" ");
        if (first != std::wstring::npos) {
            size_t last = current.find_last_not_of(L" ");
            args.push_back(current.substr(first, (last - first + 1)));
        }
    }
    return args;
}

std::wstring vShellEngine::processArgument(std::wstring arg) {
    if (arg.empty()) return L"";

    // 1. Detectăm dacă este string citat (Ex: " B")
    // Acum arg va fi fix "\" B\"", fără spații înainte de prima ghilimea
    bool isQuoted = (arg.size() >= 2 && arg.front() == L'\"' && arg.back() == L'\"');

    // 2. DACĂ NU E CITAT, normalizăm spațiile (pentru comenzi, funcții, etc)
    if (!isQuoted) {
        arg = normalizeSpaces(arg);
    }

    // 3. Logica de funcții
    size_t openParen = arg.find(L'(');
    size_t closeParen = arg.find_last_of(L')');

    if (openParen != std::wstring::npos && closeParen != std::wstring::npos && openParen < closeParen) {
        std::wstring funcName = arg.substr(0, openParen);
        // Trim la numele funcției (ex: "SUM " -> "SUM")
        size_t fLast = funcName.find_last_not_of(L" ");
        if (fLast != std::wstring::npos) funcName = funcName.substr(0, fLast + 1);

        if (m_functionHandlers.count(funcName)) {
            std::wstring innerContent = arg.substr(openParen + 1, closeParen - openParen - 1);
            std::vector<std::wstring> rawArgs = splitArguments(innerContent);
            std::vector<std::wstring> processedArgs;

            for (auto& rawArg : rawArgs) {
                processedArgs.push_back(processArgument(rawArg));
            }
            return m_functionHandlers[funcName](processedArgs);
        }
    }

    // 4. Substituție variabile
    // IMPORTANT: substituteVariables trebuie să știe să nu strice ghilimelele (dar de obicei e ok)
    arg = substituteVariables(arg, m_variables);

    // 5. Evaluare matematică - DOAR dacă nu suntem în ghilimele
    if (!isQuoted && arg.find_first_of(L"+-*/^") != std::wstring::npos) {
        try {
            std::string mathExpr(arg.begin(), arg.end());
            return L"evaluate_formula_fp(mathExpr)";
        }
        catch (...) { return arg; }
    }

    return arg;
}


    
    
    void vShellEngine::handleInput(const std::wstring& line, std::wstring& accumulator) {
        // Dacă linia începe cu '/', este o comandă administrativă instantanee
        if (line[0] == L'/') {
            executeShellCommand(line);
            return;
        }

        // Altfel, acumulăm pentru SQL
        accumulator += line + L" ";
        if (line.find(L';') != std::wstring::npos) {
            
            accumulator.clear();
        }
    }
    
void vShellEngine::addToHistory(const std::wstring& query) {
        m_history.push_back(query);
        // Salvare opțională în fișier
        std::wofstream historyFile("vSql_history.txt", std::ios::app);
        if (historyFile.is_open()) {
            historyFile << query << std::endl;
        }
    }



std::wstring dataToString(const vDataValue & data) {
    if (std::holds_alternative<std::wstring>(data))
        return std::get<std::wstring>(data);
    if (std::holds_alternative<long long>(data))
        return std::to_wstring(std::get<long long>(data));
    if (std::holds_alternative<vDataArray>(data))
        return L"[Array]"; // Sau o logică de serializare [1, 2, 3]
        
    return L"";
}

std::wstring vShellEngine::substituteVariables(std::wstring query, const std::map<std::wstring, vData>& vars) {
    std::wstring result;
    size_t i = 0;

    while (i < query.length()) {
        if (query[i] == L'$') {
            size_t start = i;
            i++; // trecem de $

            // 1. Extragem numele variabilei
            std::wstring varName = L"$";
            while (i < query.length() && (std::iswalnum(query[i]) || query[i] == L'_')) {
                varName += query[i++];
            }

            // 2. Verificăm existența
            auto it = vars.find(varName);
            if (it != vars.end()) {
                // FIX: it->second este vData, deci luăm .value (care este vDataValue)
                vDataValue currentVal = it->second.value;

                // 3. LOGICA DE INDEXARE
                while (i < query.length() && query[i] == L'[') {
                    size_t closeBracket = query.find(L']', i);
                    if (closeBracket != std::wstring::npos) {
                        std::wstring indexStr = query.substr(i + 1, closeBracket - i - 1);

                        try {
                            // Recursivitate pentru index (permite $A[$i])
                            std::wstring resolvedIdx = substituteVariables(indexStr, vars);
                            int idx = std::stoi(resolvedIdx);

                            if (std::holds_alternative<vDataArray>(currentVal)) {
                                const auto& arr = std::get<vDataArray>(currentVal);
                                if (idx >= 0 && (size_t)idx < arr.size()) {
                                    // Mergem un nivel mai adânc
                                    currentVal = arr[idx].value;
                                }
                                else {
                                    currentVal = std::monostate{}; // Out of bounds
                                }
                            }
                            else {
                                currentVal = std::monostate{}; // Nu este array, dar s-a încercat indexarea
                            }
                        }
                        catch (...) {
                            currentVal = std::monostate{}; // Eroare la conversia indexului
                        }

                        i = closeBracket + 1;
                    }
                    else break;
                }
                result += dataToString(currentVal);
            }
            else {
                result += query.substr(start, i - start);
            }
        }
        else {
            result += query[i++];
        }
    }
    return result;
}




    
    

    std::wstring vShellEngine::normalizeVarName(std::wstring name) {
        name.erase(name.find_last_not_of(L" ") + 1);
        name.erase(0, name.find_first_not_of(L" "));
        if (!name.empty() && name[0] != L'$') name = L"$" + name;
        return name;
    }

   
    
    std::wstring vShellEngine::vDataToPrintable(const vDataValue& data) {
        if (std::holds_alternative<std::wstring>(data)) {
            // Dacă e wstring, îl afișăm cu ghilimele ca să știm că e text
            return L"\"" + std::get<std::wstring>(data) + L"\"";
        }
        if (std::holds_alternative<long long>(data)) {
            return std::to_wstring(std::get<long long>(data));
        }
        if (std::holds_alternative<double>(data)) {
            std::wstring s = std::to_wstring(std::get<double>(data));
            // Curățăm zerourile inutile: 2.500000 -> 2.5
            if (s.find(L'.') != std::wstring::npos) {
                s.erase(s.find_last_not_of(L'0') + 1, std::wstring::npos);
                if (s.back() == L'.') s.pop_back(); // Eliminăm punctul dacă nu mai avem zecimale
            }
            return s;
        }
        if (std::holds_alternative<bool>(data)) {
            return std::get<bool>(data) ? L"true" : L"false";
        }
        if (std::holds_alternative<vDataArray>(data)) {
            const auto& arr = std::get<vDataArray>(data);
            std::wstring res = L"[";
            for (size_t i = 0; i < arr.size(); ++i) {
                res += vDataToPrintable(arr[i].value);
                if (i < arr.size() - 1) res += L", ";
            }
            res += L"]";
            return res;
        }
        return L"null";
    }

    bool vShellEngine::displayVariables() {
        if (m_variables.empty()) {
            //LOG_INFO(L"No variables defined.");
            return false;
        }
        else {
            ConsoleManager::getInstance().writeRaw(L"=== Defined Variables ===\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            for (const auto& [name, dataObj] : m_variables) {
                // dataObj este de tip vData, deci accesăm .value
                std::wstring printableValue = vDataToPrintable(dataObj.value);
                ConsoleManager::getInstance().writeRaw(name + L" = " + printableValue + L"\n");
            }

        }
        return true;
    }

    bool vShellEngine::displaySingleVariable(const std::wstring& varName) {
        std::wstring cleanName = normalizeVarName(varName);

        auto it = m_variables.find(cleanName);
        if (it != m_variables.end()) {
            // it->second este obiectul vData. 
            // Folosim funcția de serializare pentru a transforma .value în text.
            std::wstring printableValue = vDataToPrintable(it->second.value);

            ConsoleManager::getInstance().writeRaw(
                cleanName + L" = " + printableValue + L"\n",
                FOREGROUND_GREEN | FOREGROUND_INTENSITY
            );
        }
        else {
            LOG_ERROR(L"Variable " + cleanName + L" is not defined.");
            return false;
        }
        return true;
    }


    vData vShellEngine::resolveExpression(std::wstring input) {
        input = normalizeSpaces(input); // Curățăm spațiile inutile
        if (input.empty()) return vData{};

        // 1. Cazul LITERAl ARRAY: [ "a", "b" ]
        if (input.front() == L'[') {
            size_t pos = 0;
            return parseLiteral(input, pos);
        }

        // 2. Cazul SUB-COMANDĂ: ` /eval ... `
        if (input.size() >= 2 && input.front() == L'`' && input.back() == L'`') {
            std::wstring innerCmd = input.substr(1, input.size() - 2);

            // Executăm comanda internă
            // Sfat: executeShellCommand ar trebui să pună rezultatul în $LAST_RESULT
            executeShellCommand(innerCmd);

            // Dacă e /eval, returnăm valoarea calculată
            if (innerCmd.find(L"/eval") != std::wstring::npos) {
                auto it = m_variables.find(L"$LAST_EVAL");
                if (it != m_variables.end()) return vData{ it->second };
            }

            // Altfel, returnăm rezultatul generic al ultimei comenzi
            auto it = m_variables.find(L"$LAST_RESULT");
            if (it != m_variables.end()) return vData{ it->second };
        }

        
        // 3. Cazul VARIABILĂ PURĂ (Deep Copy)
        input = normalizeSpaces(input);

        // Verificăm dacă input-ul este strict o variabilă (ex: "$a")
        // IMPORTANT: Facem asta ÎNAINTE de substituteVariables
        if (!input.empty() && input.front() == L'$') {
            // Dacă input-ul este pur o variabilă sau variabilă cu index ($a sau $a[0])
            // și nu are alte caractere (ca + sau spații)
            if (input.find_first_of(L" +-*/") == std::wstring::npos) {
                // --- LOGICĂ REPARATĂ ---
        // 1. Încercăm rezolvarea (inclusiv indexare gen $a[0])
                vDataValue val = resolveVariableToValue(input, m_variables);

                if (!std::holds_alternative<std::monostate>(val)) {
                    vData res;
                    res.value = val; // Aici se copiază vectorul direct (Deep Copy)
                    return res;
                }
                else {
                    // Dacă resolveVariableToValue a eșuat, verificăm dacă nu cumva e o 
                    // variabilă simplă care în map e stocată FĂRĂ '$'
                    std::wstring cleanName = input.substr(1); // scoatem $
                    auto it = m_variables.find(cleanName);
                    if (it != m_variables.end()) {
                        vData res;
                        res.value = it->second.value;
                        return res;
                    }
                }
            }
        }

        // 4. Doar dacă NU a fost o variabilă pură, mergem pe logica de string-uri
        std::wstring substituted = substituteVariables(input, m_variables);
        vData res;
        res.value = parseLiteralToValue(substituted);
        return res;
        
        /*
        // 3. Cazul STRING simplu sau VARIABLE SUBSTITUTION
        // Dacă nu e array sau sub-comanda, este un string care poate conține variabile
        std::wstring substituted = substituteVariables(input, m_variables);
        vData res;
        res.value = parseLiteralToValue(substituted);
        return res;
        */
    }

   

    

    
    vDataValue vShellEngine::getVarValue(std::wstring expression) {
        if (expression.empty() || expression[0] != L'$') return std::monostate{};

        // 1. Găsim unde se termină numele variabilei (la prima paranteză '[' sau la final)
        size_t bracketPos = expression.find(L'[');
        std::wstring varName = (bracketPos == std::wstring::npos) ? expression : expression.substr(0, bracketPos);

        // Căutăm variabila de bază
        auto it = m_variables.find(varName);
        if (it == m_variables.end()) return std::monostate{};

        vDataValue currentVal = it->second.value;

        // 2. Navigăm prin indexări dacă există (ex: [1][1])
        if (bracketPos != std::wstring::npos) {
            std::wstring indexPart = expression.substr(bracketPos);
            size_t pos = 0;

            while ((pos = indexPart.find(L'[', pos)) != std::wstring::npos) {
                size_t endPos = indexPart.find(L']', pos);
                if (endPos == std::wstring::npos) break;

                // Extragem ce e între [ ]
                std::wstring idxStr = indexPart.substr(pos + 1, endPos - pos - 1);

                // Evaluăm indexul (poate fi o variabilă gen $i)
                int idx = 0;
                try {
                    std::wstring resolvedIdx = substituteVariables(idxStr, m_variables);
                    idx = std::stoi(resolvedIdx);
                }
                catch (...) { return std::monostate{}; }

                // Verificăm dacă valoarea curentă este un Array pentru a putea accesa indexul
                if (std::holds_alternative<vDataArray>(currentVal)) {
                    const auto& arr = std::get<vDataArray>(currentVal);
                    if (idx >= 0 && (size_t)idx < arr.size()) {
                        currentVal = arr[idx].value; // "Săpăm" un nivel mai jos
                    }
                    else {
                        return std::monostate{}; // Index out of bounds
                    }
                }
                else {
                    return std::monostate{}; // Nu putem indexa ceva ce nu e Array
                }

                pos = endPos + 1; // Mergem la următoarea pereche de paranteze
            }
        }

        return currentVal;
    }
    

    vDataValue vShellEngine::parseLiteralToValue(std::wstring str) {
        str = normalizeSpaces(str);
        if (str.empty()) return std::monostate{};

        // 1. String literar marcat cu ghilimele: "123" -> rămâne string
        if (str.size() >= 2 && str.front() == L'\"' && str.back() == L'\"') {
            return str.substr(1, str.size() - 2);
        }

        // 2. Variabilă: $X -> extragem valoarea din memorie
        if (str.front() == L'$') {
            return getVarValue(str);
        }

        // 3. Valori Booleane
        if (str == L"true") return true;
        if (str == L"false") return false;

        // 4. Încercăm conversia numerică (Inima rezolvării tale)
        try {
            size_t processed = 0;
            // Cazul Double
            if (str.find(L'.') != std::wstring::npos) {
                double d = std::stod(str, &processed);
                if (processed == str.size()) return d;
            }
            // Cazul Integer
            else {
                long long ll = std::stoll(str, &processed);
                if (processed == str.size()) return ll;
            }
        }
        catch (...) {
            // Nu este număr valid, mergem mai departe
        }

        // 5. Fallback: Dacă nu e nimic de mai sus, e un string brut (ex: numele unei tabele)
        return str;
    }

    vData vShellEngine::parseLiteral(const std::wstring& input, size_t& pos) {
        vData result;
        // Sărim peste spațiile albe de la început
        while (pos < input.size() && iswspace(input[pos])) pos++;

        if (pos >= input.size()) return result;

        if (input[pos] == L'[') {
            // --- CAZUL 1: ARRAY ---
            vDataArray arr;
            pos++; // trecem de [
            while (pos < input.size() && input[pos] != L']') {
                // Recursivitate: elementul poate fi orice (alt array, string, numar)
                arr.push_back(parseLiteral(input, pos));

                // Sărim peste virgule și spații între elemente
                while (pos < input.size() && (iswspace(input[pos]) || input[pos] == L',')) pos++;
            }
            if (pos < input.size()) pos++; // trecem de ]
            result.value = arr;
        }
        else if (input[pos] == L'\"') {
            // --- CAZUL 2: STRING EXPLICIT (cu ghilimele) ---
            std::wstring s;
            // Păstrăm ghilimeaua de deschidere pentru ca parseLiteralToValue să știe că e string
            s += input[pos++];

            while (pos < input.size() && input[pos] != L'\"') {
                if (input[pos] == L'\\' && pos + 1 < input.size()) {
                    // Suport pentru escape character (ex: \")
                    s += input[pos++];
                }
                s += input[pos++];
            }

            // Păstrăm și ghilimeaua de închidere
            if (pos < input.size()) s += input[pos++];

            // Trimitem string-ul "3" cu tot cu ghilimele. 
            // parseLiteralToValue va scoate ghilimelele și va returna wstring pur.
            result.value = parseLiteralToValue(s);
        }
        else {
            // --- CAZUL 3: NUMĂR, BOOLEAN SAU VARIABILĂ ---
            std::wstring raw;
            // Colectăm până la separator (spațiu, virgulă sau închidere array)
            while (pos < input.size() && !iswspace(input[pos]) &&
                input[pos] != L',' && input[pos] != L']') {
                raw += input[pos++];
            }

            // Trimitem textul brut (ex: 2.5 sau true) pentru conversie
            result.value = parseLiteralToValue(raw);
        }

        return result;
    }


    vDataValue vShellEngine::resolveVariableToValue(std::wstring input, const std::map<std::wstring, vData>& vars) {
        if (input.empty()) return std::monostate{};

        // Extragem numele de bază (ex din $a[0] extragem $a)
        size_t i = 0;
        std::wstring varName;

        // Luăm tot până la prima paranteză sau spațiu
        while (i < input.length() && input[i] != L'[') {
            varName += input[i++];
        }

        // Încercăm să găsim variabila în map
        auto it = vars.find(varName);
        if (it == vars.end()) {
            // Dacă nu există cu $, încercăm fără $
            if (varName[0] == L'$') {
                it = vars.find(varName.substr(1));
            }
        }

        if (it == vars.end()) return std::monostate{}; // Nu am găsit-o deloc

        vDataValue currentVal = it->second.value;

        // Procesăm indicii dacă există ($a[0][1])
        while (i < input.length() && input[i] == L'[') {
            size_t end = input.find(L']', i);
            if (end == std::wstring::npos) break;

            std::wstring idxStr = input.substr(i + 1, end - i - 1);
            // Rezolvăm indexul (poate fi o altă variabilă gen $a[$i])
            std::wstring resolvedIdx = substituteVariables(idxStr, vars);

            try {
                int idx = std::stoi(resolvedIdx);
                if (std::holds_alternative<vDataArray>(currentVal)) {
                    vDataArray& arr = std::get<vDataArray>(currentVal);
                    if (idx >= 0 && (size_t)idx < arr.size()) {
                        // 1. Facem o copie a VALORII elementului
                        vDataValue nextLevel = arr[idx].value;
                        // 2. Abia acum suprascriem currentVal (vechiul array moare aici, dar nextLevel e safe)
                        currentVal = nextLevel;
                        //currentVal = arr[idx].value;
                    }
                    else return std::monostate{};
                }
                else return std::monostate{};
            }
            catch (...) { return std::monostate{}; }

            i = end + 1;
        }

        return currentVal;
    }
