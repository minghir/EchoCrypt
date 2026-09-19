#include <variant>

#include <filesystem>
#include <fstream>

#include "vShellEngine.hpp"
//#include "../math/vmath.hpp"
#include "../ConsoleManager.hpp"
#include "../stringUtils.hpp"


void vShellEngine::initializeCommandsHandlers() {
    /*
    m_commandHandlers[L"/info"] = [this](const auto& sc) {
        if (sc.args.empty()) return LOG_ERROR(L"Usage: /info <path>");
        m_provider.inspectState(sc.args[0]);
        };
        */
        /*
        m_commandHandlers[L"/save"] = [this](const auto& sc) {
            if (sc.args.empty()) return LOG_ERROR(L"Usage: /save <path>");
            if (m_provider.saveState(sc.args[0])) LOG_SUCCESS(L"State saved.");
            else LOG_ERROR(L"Failed to save state.");
            };
        */
    m_commandHandlers[L"/quit"] = [this](const auto& sc) {
        return handleQuitCommand(sc);
    };
    m_commandHandlers[L"/exit"] = m_commandHandlers[L"/quit"];
    m_commandHandlers[L"/q"] = m_commandHandlers[L"/quit"];

    m_commandHandlers[L"/set"] = [this](const auto& sc) {
        return handleSetCommand(sc);
    };
    m_commandHandlers[L"/s"] = m_commandHandlers[L"/set"];

    m_commandHandlers[L"/unset"] = [this](const auto& sc) {
        return handleUnsetCommand(sc);
    };
    m_commandHandlers[L"/u"] = m_commandHandlers[L"/unset"];

    m_commandHandlers[L"/unsetall"] = [this](const auto& sc) {
        return handleUnsetAllCommand(sc);
    };
    m_commandHandlers[L"/ua"] = m_commandHandlers[L"/unsetall"];

    m_commandHandlers[L"/echo"] = [this](const auto& sc) {//requery
        return handleEchoCommand(sc);
    };

    m_commandHandlers[L"/clear"] = [this](const auto& sc) {
        return handleClearCommand(sc);
    };

    m_commandHandlers[L"/history"] = [this](const auto& sc) {
        return handleHistoryCommand(sc);
    };

    m_commandHandlers[L"/run"] = [this](const auto& sc) {
        return handleRunCommand(sc);
    };

    m_commandHandlers[L"/r"] = m_commandHandlers[L"/run"];


    m_commandHandlers[L"/pause"] = [this](const auto& sc) {
        return handlePauseCommand(sc);
    };

    m_commandHandlers[L"/sys"] = [this](const auto& sc) {
        return handleSysCommand(sc);
    };

    m_commandHandlers[L"/foreach"] = [this](const auto& sc) {
        return handleForeachCommand(sc);
    };

    m_commandHandlers[L"/if"] = [this](const auto& sc) {
        return handleIfCommand(sc);
    };

    m_commandHandlers[L"/eval"] = [this](const auto& sc) {
        return handleEvalCommand(sc);
    };
    m_commandHandlers[L"/e"] = m_commandHandlers[L"/eval"];

    m_commandHandlers[L"/print_r"] = [this](const auto& sc) {
        return handlePrintRCommand(sc);
    };


    m_commandHandlers[L"/imp"] = [this](const auto& sc) {return  handleImportCommand(sc); };
    m_commandHandlers[L"/import"] = [this](const auto& sc) { return handleImportCommand(sc); };

    // Alias-uri
    m_commandHandlers[L"/h"] = [this](const auto& sc) { return showHelp(); };
    m_commandHandlers[L"/help"] = m_commandHandlers[L"/h"];

    
    //m_commandHandlers[L"/t"] = [this](const auto& sc) {
        //this->res = m_provider.getSchemaCatalog();
        //logQueryResult(this->res);
    //};

    //m_commandHandlers[L"/tables"] = m_commandHandlers[L"/t"];

}


bool vShellEngine::handleIfCommand(const ShellCommand& sc) {
    // Avem nevoie de: val1 (0), op (1), val2 (2) și măcar un token de comandă (3)
    if (sc.args.size() < 4) {
        LOG_ERROR(L"Usage: /if <v1> <op> <v2> <then_cmd...> [/else <else_cmd...>]");
        return false;
    }

    // 1. Evaluăm condiția
    std::wstring v1 = substituteVariables(sc.args[0], m_variables);
    std::wstring op = sc.args[1];
    std::wstring v2 = substituteVariables(sc.args[2], m_variables);

    // Curățăm valorile (pentru spațiile din DB)
    auto trim = [](std::wstring s) {
        s.erase(0, s.find_first_not_of(L" "));
        size_t last = s.find_last_not_of(L" ");
        if (last != std::wstring::npos) s.erase(last + 1);
        return s;
    };
    v1 = trim(v1); v2 = trim(v2);

    bool conditionMet = false;
    if (op == L"==")      conditionMet = (v1 == v2);
    else if (op == L"!=") conditionMet = (v1 != v2);

    // 2. Separăm jetoanele (tokens) pentru ramura THEN și ELSE
    std::vector<std::wstring> thenTokens;
    std::vector<std::wstring> elseTokens;
    bool inElse = false;

    for (size_t i = 3; i < sc.args.size(); ++i) {
        if (sc.args[i] == L"/else") {
            inElse = true;
            continue;
        }
        if (inElse) elseTokens.push_back(sc.args[i]);
        else thenTokens.push_back(sc.args[i]);
    }

    // 3. Reconstruim comanda care a câștigat
    const auto& winnerTokens = conditionMet ? thenTokens : elseTokens;
    if (winnerTokens.empty()) return true;

    std::wstring finalCmd;
    for (const auto& t : winnerTokens) finalCmd += t + L" ";

    // 4. Executăm rezultatul
    if (finalCmd[0] == L'/') executeShellCommand(finalCmd);
    return true;
    //else processQuery(finalCmd);
}


bool vShellEngine::showHelp() {
    auto& console = ConsoleManager::getInstance();

    console.writeRaw(L"");
    console.writeRaw(L"=== vEngine SQL Shell Help ===", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    console.writeRaw(L"Note: Standard SQL commands must end with ';'", FOREGROUND_RED | FOREGROUND_GREEN);
    console.writeRaw(L"----------------------------------------------------------------------", FOREGROUND_BLUE);

    struct HelpCmd { std::wstring cmd; std::wstring desc; };
    std::vector<HelpCmd> helpCmds = {
        { L"/h, /help",          L"Show this help screen." },
        { L"/t, /tables",        L"List all available schemas and tables." },
        { L"/d <table_name>",    L"Show table structure (Describe)." },
        { L"/requery",           L"Re-print the results of the last successful query." },
        { L"/csv <tbl> <pth>",   L"Export a specific table to CSV." },
        { L"/csv last <pth>",    L"Export the current results from memory to CSV." },
        { L"/imp <ty> <p> <t>",  L"Import data (ex: /imp dbf data.dbf public.airports)." },
        { L"/save <path>",       L"Save current engine state to file." },
        { L"/load <path>",       L"Load engine state from file." },
        { L"/info <path>",       L"Inspect a state file without loading it." },
        { L"/clear",             L"Clear the console screen." },
        { L"/sys <command>",     L"Run sys command." },
        { L"/history",           L"Show recent SQL queries." },
        { L"/r, /run <file>",    L"Execute an SQL script file." },
        { L"/set <$V=val>",      L"Set a variable (ex: /set $ID=10)." },
        { L"/set $V=`SELECT...`",L"Set variable from sub-query (backticks)." },
        { L"/set $V=[col_name]", L"Set variable from last result cell (brackets)." },
        { L"/unset <$V>",        L"Remove a variable." },
        { L"/echo <text|$V>",    L"Print text or variable values to console." },
        { L"/pause [msg]",       L"Wait for Enter key before continuing." },
        { L"/q, exit, quit",     L"Close the SQL shell." },
        { L"SELECT...",          L"Execute SQL (ex: SELECT * FROM aviation.flights;)" }
    };

    for (const auto& hc : helpCmds) {
        std::wstring line = L"  ";
        line += hc.cmd;

        // Am mărit padding-ul la 25 pentru a lăsa loc comenzii /csv cu argumente
        int padding = 25 - static_cast<int>(hc.cmd.length());
        if (padding > 0) line += std::wstring(padding, L' ');

        line += L" - " + hc.desc;
        console.writeRaw(line);
    }

    console.writeRaw(L"----------------------------------------------------------------------", FOREGROUND_BLUE);
    console.writeRaw(L"Examples:", FOREGROUND_GREEN);
    console.writeRaw(L"  SQL> SELECT * FROM aviation.pilots WHERE rank = 'Captain';");
    console.writeRaw(L"  SQL> /csv last C:\\exports\\my_result.csv");
    console.writeRaw(L"");
    return true;
}

bool vShellEngine::handleEchoCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        ConsoleManager::getInstance().writeRaw(L"\n");
        return false;
    }

    std::wstring finalOutput;

    // Dacă avem un singur argument care începe cu $, încercăm getVarValue direct
    // Asta ne permite să facem echo la Array-uri sau obiecte complexe corect
    if (sc.args.size() == 1 && sc.args[0].front() == L'$') {
        vDataValue val = getVarValue(sc.args[0]);

        // Dacă getVarValue a găsit ceva (nu e monostate)
        if (!std::holds_alternative<std::monostate>(val)) {
            // Folosim vDataToPrintable pentru a vedea array-urile ca [1, 2, 3]
            finalOutput = vDataToPrintable(val);
        }
        else {
            // Dacă nu e o variabilă validă, o tratăm ca text normal
            finalOutput = substituteVariables(sc.args[0], this->m_variables);
        }
    }
    else {
        // Cazul clasic: concatenăm argumentele și substituim variabilele în text
        std::wstring rawMessage;
        for (size_t i = 0; i < sc.args.size(); ++i) {
            rawMessage += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
        }
        finalOutput = substituteVariables(rawMessage, this->m_variables);
    }

    // Afișăm rezultatul
    ConsoleManager::getInstance().writeRaw(finalOutput + L"\n",
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
	return true;
}

bool vShellEngine::handleImportCommand(const ShellCommand& sc) {

    /*
        if (sc.args.size() >= 2) {
            std::wstring type = sc.args[0];
            std::wstring path = sc.args[1];
            std::wstring target = (sc.args.size() >= 3) ? sc.args[2] : L"";

            LOG_INFO(L"Importing " + type + L" from " + path + L"...");
            if (m_provider.importData(type, path, target)) {
                LOG_SUCCESS(L"Import successful.");
            }
            else {
                LOG_ERROR(L"Import failed.");
            }
        }
        else {
            LOG_ERROR(L"Usage: /imp <type> <path> [target]");
        }
        */
	return true;
}

bool vShellEngine::handleUnsetCommand(const ShellCommand& sc) {

    if (!sc.args.empty()) {
        std::wstring varName = sc.args[0];

        // Ne asigurăm că verificăm și varianta cu $ în față
        if (varName[0] != L'$') varName = L"$" + varName;

        if (m_variables.erase(varName)) {
            LOG_SUCCESS(L"Variable " + varName + L" has been removed.");
            return true;
        }
        else {
            LOG_ERROR(L"Variable " + varName + L" not found.");
            return false;
        }
    }
    else {
        LOG_ERROR(L"Usage: /unset <variable_name>");
        return false;
    }

}

bool vShellEngine::handleUnsetAllCommand(const ShellCommand& sc) {

    if (m_variables.empty()) {
        //LOG_INFO(L"Memory is already clean. No variables defined.");
       
    }

    // Centură de siguranță: dacă utilizatorul pune un "-f" (force), nu mai întrebăm
    bool force = (!sc.args.empty() && sc.args[0] == L"-f");

    if (!force) {
        // Opțional: Poți cere o confirmare sau pur și simplu să execuți.
        // Pentru un shell de productivitate, execuția directă e de obicei preferată.
    }

    size_t count = m_variables.size();
    m_variables.clear();
    LOG_SUCCESS(L"Cleanup complete. Deleted " + std::to_wstring(count) + L" variables.");
    return true;
}

bool vShellEngine::handleClearCommand(const ShellCommand& sc) {

    ConsoleManager::getInstance().clear();
    return true;
}


bool vShellEngine::handleHistoryCommand(const ShellCommand& sc) {

    if (m_history.empty()) {
        //LOG_INFO(L"Command history is empty.");
    }

    auto& console = ConsoleManager::getInstance();
    console.writeRaw(L"=== Command History ===", FOREGROUND_GREEN);
    for (size_t i = 0; i < m_history.size(); ++i) {
        std::wstring entry = std::to_wstring(i + 1) + L": " + m_history[i];
        console.writeRaw(entry);
    }
    return true;
}

bool vShellEngine::handleRunCommand(const ShellCommand& sc) {

    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: /run <file.sql>");
        return false;
    }
    std::wstring filePath = sc.args[0];
    //std::wifstream file(filePath); // Mai simplu direct în constructor
    std::filesystem::path p(filePath);
    std::wifstream file(p); // Acum parantezele rotunde funcționează perfect!

    if (!file.is_open()) {
        LOG_ERROR(L"Could not open script file: " + filePath);
        return false;
    }

    auto& console = ConsoleManager::getInstance();
    //LOG_INFO(L"--- Starting Script: " + filePath + L" ---");

    std::wstring line;
    std::wstring batchQuery;
    std::wstring shellAccumulator;
    int commandCount = 0;

    while (std::getline(file, line)) {
        // 1. Curățăm spațiile și caracterele invizibile
        if (line.empty()) continue;
        line.erase(0, line.find_first_not_of(L" \t\r\n"));
        size_t last = line.find_last_not_of(L" \t\r\n");
        if (last != std::wstring::npos) line.erase(last + 1);
        else { line.clear(); continue; } // Linia era doar whitespace

        // 2. Comentarii
        if (line.empty() || line.find(L"--") == 0) continue;

        // 3. Logică continuare linie
        bool hasContinuation = (!line.empty() && line.back() == L'\\');
        if (hasContinuation) {
            line.pop_back();
        }

        // --- FIX PENTRU CRASH ---
        // Verificăm dacă suntem în mijlocul unei comenzi shell SAU dacă linia nouă începe cu '/'
        // Folosim !line.empty() înainte de a verifica line[0]
        bool isShell = !shellAccumulator.empty() || (!line.empty() && line[0] == L'/');

        if (isShell) {
            shellAccumulator += line + L" ";

            if (!hasContinuation) {
                commandCount++;
                executeShellCommand(shellAccumulator);
                shellAccumulator.clear();
            }
            continue;
        }

        // 5. Gestionare SQL
        batchQuery += line + L" ";
        // O comandă SQL se termină dacă are ';' și NU are '\' (continuație) la final
        if (!hasContinuation && line.find(L';') != std::wstring::npos) {
            commandCount++;
            // Aici ar trebui să apelezi processQuery(batchQuery);
            batchQuery.clear();
        }
    }

    LOG_SUCCESS(L"--- Script Finished (" + std::to_wstring(commandCount) + L" commands) ---");
    return true;
}

bool vShellEngine::handlePauseCommand(const ShellCommand& sc) {

    std::wstring msg = L"Press Enter to continue...";
    if (!sc.args.empty()) {
        // Dacă utilizatorul a pus un mesaj custom: /pause Verifică datele de mai sus!
        msg.clear();
        for (const auto& arg : sc.args) msg += arg + L" ";
    }

    ConsoleManager::getInstance().writeRaw(msg, FOREGROUND_RED | FOREGROUND_INTENSITY);
    std::wstring dummy;
    std::getline(std::wcin, dummy); // Așteaptă input
    return true;
}


bool vShellEngine::handleSysCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: /sys <system_command>");
        return false;
    }

    // 1. Reconstruim comanda și interpolăm variabilele shell-ului
    std::wstring fullCommand;
    for (const auto& arg : sc.args) fullCommand += arg + L" ";
    fullCommand = substituteVariables(fullCommand, m_variables);

    // Curățăm ghilimelele dacă utilizatorul a scris /sys "ls -l"
    if (fullCommand.front() == L'"' && fullCommand.back() == L'"') {
        fullCommand = fullCommand.substr(1, fullCommand.size() - 2);
    }

    //LOG_INFO(L"Executing system command: " + fullCommand);

    // 2. Executăm și capturăm output-ul
    // Folosim _wpopen pentru a suporta caractere wide (Unicode)
    FILE* pipe = _wpopen(fullCommand.c_str(), L"rt");
    if (!pipe) {
        LOG_ERROR(L"Could not execute system command.");
        return false;
    }

    wchar_t buffer[128];
    while (fgetws(buffer, 128, pipe)) {
        std::wcout << buffer;
    }

    int returnCode = _pclose(pipe);

    if (returnCode == 0) {
        LOG_SUCCESS(L"Command finished.");
        return true;
    }
    else {
        LOG_ERROR(L"Command failed with code: " + std::to_wstring(returnCode));
        return false;
    }

}


bool vShellEngine::handleEvalCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: /eval <expression> (ex: /eval FACT(3) * 2)");
        return false;
    }

    // 1. Reconstruim expresia din argumente
    std::wstring wideExpr;
    for (size_t i = 0; i < sc.args.size(); ++i) {
        wideExpr += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");
    }

    try {
        // 2. Procesăm argumentul: rezolvăm Funcții (FACT) -> Variabile ($A) -> Matematică
        // Folosim processArgument deoarece acesta face deja substitutia si calculele matematice
        std::wstring processedResult = processArgument(wideExpr);

        // 3. Afișăm rezultatul
        ConsoleManager::getInstance().writeRaw(processedResult + L"\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        // 4. Salvăm rezultatul în $LAST_EVAL sub formă de vData (pentru a fi folosit în /set)
        vData resObj;
        resObj.value = processedResult;
        m_variables[L"$LAST_EVAL"] = resObj;

    }
    catch (const std::exception& e) {
        LOG_ERROR(L"Eval Error: " + std::wstring(e.what(), e.what() + strlen(e.what())));
        return false;
    }
    catch (...) {
        LOG_ERROR(L"Eval Error: A apărut o eroare neașteptată în timpul evaluării.");
        return true;
    }
}


bool vShellEngine::handleQuitCommand(const ShellCommand& sc) {
    dbfConn->closeDatabase();
    return stop();
}

std::wstring vShellEngine::formatPrintR(const vDataValue& data, int indent) {
    std::wstring space(indent * 4, L' '); // 4 spații per nivel
    std::wstring result;

    if (std::holds_alternative<vDataArray>(data)) {
        const auto& arr = std::get<vDataArray>(data);
        result += L"Array\n" + space + L"(\n";

        for (size_t i = 0; i < arr.size(); ++i) {
            result += space + L"    [" + std::to_wstring(i) + L"] => ";
            // Apel recursiv pentru elementele din array
            result += formatPrintR(arr[i].value, indent + 1);
            result += L"\n";
        }

        result += space + L")";
    }
    else if (std::holds_alternative<std::wstring>(data)) {
        result += std::get<std::wstring>(data);
    }
    else if (std::holds_alternative<long long>(data)) {
        result += std::to_wstring(std::get<long long>(data));
    }
    else if (std::holds_alternative<double>(data)) {
        result += std::to_wstring(std::get<double>(data));
    }
    else if (std::holds_alternative<bool>(data)) {
        result += std::get<bool>(data) ? L"1" : L""; // PHP stil: true e 1, false e gol
    }
    else {
        result += L"NULL";
    }

    return result;
}

bool vShellEngine::handlePrintRCommand(const ShellCommand& sc) {
    if (sc.args.empty()) {
        LOG_ERROR(L"Usage: /print_r <variable_name>");
        return false;
    }

    // Luăm numele variabilei (ex: $A sau $A[0])
    std::wstring varPath = sc.args[0];

    // Folosim o logică similară cu substituteVariables dar care returnează obiectul vData direct
    // Pentru simplitate aici, dacă e doar un nume de variabilă:
    std::wstring cleanName = normalizeVarName(varPath);

    auto it = m_variables.find(cleanName);
    if (it != m_variables.end()) {
        std::wstring output = formatPrintR(it->second.value, 0);
        ConsoleManager::getInstance().writeRaw(output + L"\n", FOREGROUND_GREEN | FOREGROUND_INTENSITY);
    }
    else {
        LOG_ERROR(L"Variable " + cleanName + L" not found.");
        return false;
    }
    return true;
}


bool vShellEngine::handleForeachCommand(const ShellCommand& sc) {
    /*
    if (this->res.data.empty()) return;

    std::wstring fullTemplate;
    for (const auto& arg : sc.args) fullTemplate += arg + L" ";

    for (const auto& row : this->res.data) {
        std::map<std::wstring, std::wstring> rowContext = m_variables;
        for (size_t i = 0; i < this->res.aliases.size(); ++i) {
            std::wstring val = vValueToString(row[i]);
            // Trim obligatoriu pentru comparatii corecte
            val.erase(0, val.find_first_not_of(L" "));
            val.erase(val.find_last_not_of(L" ") + 1);
            rowContext[L"$" + this->res.aliases[i]] = val;
        }

        std::wstring interpolatedLine = substituteVariables(fullTemplate, rowContext);

        // --- NOUA LOGICĂ DE SPLIT INTELIGENT ---
        std::vector<std::wstring> finalCommands;
        size_t pos = 0;
        while ((pos = interpolatedLine.find(L'/', pos)) != std::wstring::npos) {
            size_t nextPos = interpolatedLine.find(L'/', pos + 1);

            // Verificăm dacă / urmat de ceva este de fapt un /else
            // Dacă da, "sărim" peste el și căutăm următorul /
            while (nextPos != std::wstring::npos) {
                std::wstring peek = interpolatedLine.substr(nextPos, 6); // Verificăm dacă e "/else "
                if (peek.find(L"/else") == 0) {
                    nextPos = interpolatedLine.find(L'/', nextPos + 1);
                }
                else {
                    break;
                }
            }

            std::wstring cmd = (nextPos == std::wstring::npos)
                ? interpolatedLine.substr(pos)
                : interpolatedLine.substr(pos, nextPos - pos);

            finalCommands.push_back(cmd);
            if (nextPos == std::wstring::npos) break;
            pos = nextPos;
        }

        for (auto& cmd : finalCommands) {
            cmd.erase(0, cmd.find_first_not_of(L" "));
            cmd.erase(cmd.find_last_not_of(L" ") + 1);
            if (cmd.empty()) continue;

            if (cmd[0] == L'/') executeShellCommand(cmd);
            else processQuery(cmd);
        }
    }
    */
    return true;
}

/*
void vShellEngine::handleSetCommand(const ShellCommand& sc) {
    if (sc.args.empty()) { displayVariables(); return; }

    // 1. Reconstrucție linie și split la '='
    std::wstring fullLine;
    for (size_t i = 0; i < sc.args.size(); ++i)
        fullLine += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");

    size_t eqPos = fullLine.find(L'=');
    if (eqPos == std::wstring::npos) { displaySingleVariable(fullLine); return; }

    std::wstring rawName = fullLine.substr(0, eqPos);
    std::wstring rawValue = fullLine.substr(eqPos + 1);

    // Curățăm valoarea (partea dreaptă)
    rawValue.erase(0, rawValue.find_first_not_of(L" "));
    rawValue.erase(rawValue.find_last_not_of(L" ") + 1);

    // Rezolvăm valoarea folosind noul resolveExpression care știe de array-uri și backticks
    vData finalValue = resolveExpression(rawValue);

    // 2. LOGICA DE STÂNGA (Unde salvăm?)
    size_t bracketPos = rawName.find(L'[');

    if (bracketPos != std::wstring::npos) {
        // CAZ: /set $A[0][1] = valoare
        std::wstring baseVarName = normalizeVarName(rawName.substr(0, bracketPos));
        std::wstring indexPart = rawName.substr(bracketPos);

        auto it = m_variables.find(baseVarName);
        if (it != m_variables.end()) {
            // Extragem indicii (ex: [0][1] -> {0, 1})
            std::vector<int> indices;
            size_t p = 0;
            while ((p = indexPart.find(L'[', p)) != std::wstring::npos) {
                size_t endP = indexPart.find(L']', p);
                if (endP != std::wstring::npos) {
                    std::wstring idxStr = indexPart.substr(p + 1, endP - p - 1);
                    indices.push_back(std::stoi(substituteVariables(idxStr, m_variables)));
                    p = endP + 1;
                }
                else break;
            }

            // Navigăm în adâncime până la ultimul nivel
            vDataValue* current = &(it->second.value);
            bool possible = true;
            for (size_t i = 0; i < indices.size() - 1; ++i) {
                if (std::holds_alternative<vDataArray>(*current)) {
                    vDataArray& arr = std::get<vDataArray>(*current);
                    if (indices[i] >= 0 && (size_t)indices[i] < arr.size()) {
                        current = &(arr[indices[i]].value);
                    }
                    else { possible = false; break; }
                }
                else { possible = false; break; }
            }

            // Aplicăm modificarea la indexul final
            if (possible && std::holds_alternative<vDataArray>(*current)) {
                vDataArray& arr = std::get<vDataArray>(*current);
                int finalIdx = indices.back();
                if (finalIdx >= 0 && (size_t)finalIdx < arr.size()) {
                    arr[finalIdx] = finalValue;
                    LOG_SUCCESS(L"Element " + rawName + L" updated.");
                }
                else LOG_ERROR(L"Index out of bounds.");
            }
            else LOG_ERROR(L"Target is not an array or path is invalid.");
        }
        else LOG_ERROR(L"Variable " + baseVarName + L" not found.");
    }
    else {
        // CAZ NORMAL: /set $A = valoare
        std::wstring varName = normalizeVarName(rawName);
        m_variables[varName] = finalValue;
        LOG_SUCCESS(L"Variable " + varName + L" set.");
    }
}
*/

bool vShellEngine::handleSetCommand(const ShellCommand& sc) {
    if (sc.args.empty()) { displayVariables(); return true; }

    // Reconstruim linia pentru a găsi '=' corect
    std::wstring fullLine;
    for (size_t i = 0; i < sc.args.size(); ++i)
        fullLine += sc.args[i] + (i < sc.args.size() - 1 ? L" " : L"");

    size_t eqPos = fullLine.find(L'=');
    if (eqPos == std::wstring::npos) { return displaySingleVariable(normalizeVarName(fullLine)); }

    std::wstring rawName = normalizeSpaces(fullLine.substr(0, eqPos));
    // Dacă numele începe cu $, îl curățăm manual ca să obținem cheia din map
    if (!rawName.empty() && rawName[0] == L'$') {
        // Verificăm dacă NU este o indexare (nu are '[')
        if (rawName.find(L'[') == std::wstring::npos) {
            rawName = rawName.substr(1); // Scoatem $ și rămâne doar "a"
        }
    }

    std::wstring rawValue = normalizeSpaces(fullLine.substr(eqPos + 1));

   

    // Folosim resolveExpression - e inima sistemului tău
    vData finalValue = resolveExpression(rawValue);

    size_t bracketPos = rawName.find(L'[');
    if (bracketPos != std::wstring::npos) {
        // --- LOGICĂ INDEXARE COMPLEXĂ ---
        std::wstring baseVarName = normalizeVarName(rawName.substr(0, bracketPos));
        std::wstring indexPart = rawName.substr(bracketPos);

        auto it = m_variables.find(baseVarName);
        if (it == m_variables.end()) {
            LOG_ERROR(L"Variable " + baseVarName + L" not found.");
            return false;
        }

        // Extragere indici cu protecție
        std::vector<int> indices;
        try {
            size_t p = 0;
            while ((p = indexPart.find(L'[', p)) != std::wstring::npos) {
                size_t endP = indexPart.find(L']', p);
                if (endP == std::wstring::npos) break;
                std::wstring idxStr = indexPart.substr(p + 1, endP - p - 1);

                indices.push_back(std::stoi(substituteVariables(idxStr, m_variables)));
                /*
                vData valueIdx = resolveExpression(idxStr);
                if (std::holds_alternative<long long>(valueIdx.value)) {
                    indices.push_back((int)std::get<long long>(valueIdx.value));
                }
                else if (std::holds_alternative<std::wstring>(valueIdx.value)) {
                    // Fallback dacă e string, dar măcar e controlat
                    indices.push_back(std::stoi(std::get<std::wstring>(valueIdx.value)));
                }
                */
                p = endP + 1;
            }
        }
        catch (...) {
            LOG_ERROR(L"Invalid index format in " + rawName);
            return false;
        }

        if (indices.empty()) return false;

        // Navigare în adâncime
        vDataValue* current = &(it->second.value);
        for (size_t i = 0; i < indices.size() - 1; ++i) {
            if (std::holds_alternative<vDataArray>(*current)) {
                vDataArray& arr = std::get<vDataArray>(*current);
                int idx = indices[i];
                if (idx >= 0 && (size_t)idx < arr.size()) {
                    current = &(arr[idx].value);
                }
                else { LOG_ERROR(L"Index out of bounds."); return false; }
            }
            else { LOG_ERROR(L"Path component is not an array."); return false; }
        }


        // Modificarea finală
        if (std::holds_alternative<vDataArray>(*current)) {
            vDataArray& arr = std::get<vDataArray>(*current);
            int finalIdx = indices.back();
            if (finalIdx >= 0 && (size_t)finalIdx < arr.size()) {
                arr[finalIdx] = finalValue;
                LOG_SUCCESS(L"Updated: " + rawName);
            }
            else {
                LOG_ERROR(L"Final index out of bounds.");
                return false;
            }
        }
        else {
            LOG_ERROR(L"Target is not an array.");
            return false;
        }
    }
    else {
        // --- ATRIBUIRE SIMPLĂ ---
        m_variables[normalizeVarName(rawName)] = finalValue;
        LOG_SUCCESS(L"Set: " + normalizeVarName(rawName));
        return true;
    }
    return true;
}