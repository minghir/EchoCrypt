#include <variant>

#include <filesystem>
#include <fstream>

#include "../shell/vShellEngine.hpp"
//#include "../math/vmath.hpp"
#include "../ConsoleManager.hpp"
#include "../stringUtils.hpp"



void vShellEngine::initializeFunctionsHandlers() {
    m_functionHandlers[L"TYPEOF"] = [this](const auto& args) { return fn_TYPEOF(args); };

    m_functionHandlers[L"GET_VAR_VAL"] = [this](const auto& args) { return fn_GET_VAR_VAL(args); };
    m_functionHandlers[L"SET_VAR_VAL"] = [this](const auto& args) { return fn_SET_VAR_VAL(args); };

    m_functionHandlers[L"PUSH"] = [this](const auto& args) { return fn_PUSH(args); };
    m_functionHandlers[L"POP"] = [this](const auto& args) { return fn_POP(args); };

    m_functionHandlers[L"SUM"] = [this](const auto& args) { return fn_SUM(args); };
    
    m_functionHandlers[L"CONCAT"] = [this](const auto& args) { return fn_CONCAT(args); };
    m_functionHandlers[L"UPPER"] = [this](const auto& args) { return fn_UPPER(args); };
    m_functionHandlers[L"LEN"] = [this](const auto& args) { return fn_LEN(args); };
}

std::wstring vShellEngine::fn_TYPEOF(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"UNDEFINED";

    std::wstring val = args[0];

    // Dacă valoarea a fost deja substituită (ex: "1"), trebuie să vedem ce este "1"
    // Încercăm să parsăm valoarea primită ca pe un literal
    size_t pos = 0;
    vData data = parseLiteral(val, pos);

    // Dacă parseLiteral eșuează sau returnează string, dar noi vrem să fim siguri
    // că verificăm și variabilele care n-au fost încă substituite (just in case):
    if (data.isString()) {
        std::wstring strVal = std::get<std::wstring>(data.value);
        if (m_variables.count(strVal)) {
            data = m_variables[strVal];
        }
    }

    if (data.isArray())  return L"ARRAY";
    if (data.isMap())    return L"MAP";
    if (std::holds_alternative<long long>(data.value)) return L"INTEGER";
    if (std::holds_alternative<double>(data.value))    return L"DOUBLE";
    if (std::holds_alternative<bool>(data.value))      return L"BOOLEAN";
    if (data.isString()) return L"STRING";

    return L"UNKNOWN";
}


std::wstring vShellEngine::fn_SUM(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"0";
    double total = 0.0;
    for (const auto& arg : args) {
        try {
            size_t processedChars = 0;
            double val = std::stod(arg, &processedChars);
            if (processedChars < arg.length()) {
                std::wstring remaining = arg.substr(processedChars);
                if (remaining.find_first_not_of(L" ") != std::wstring::npos) return L"NaN";
            }
            total += val;
        }
        catch (...) { return L"NaN"; }
    }
    std::wstring result = std::to_wstring(total);
    if (result.find(L'.') != std::wstring::npos) {
        result.erase(result.find_last_not_of(L'0') + 1, std::wstring::npos);
        if (result.back() == L'.') result.pop_back();
    }
    return result;
}

std::wstring vShellEngine::fn_UPPER(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"";
    std::wstring s = args[0];
    std::transform(s.begin(), s.end(), s.begin(), ::towupper);
    return s;
}


std::wstring vShellEngine::stripQuotes(std::wstring s) {
    // Dacă avem " 2", substr(1, size-2) va returna exact " 2" (cu spațiu)
    if (s.size() >= 2 && s.front() == L'\"' && s.back() == L'\"') {
        return s.substr(1, s.size() - 2);
    }
    return s;
}


std::wstring vShellEngine::fn_CONCAT(const std::vector<std::wstring>& args) {
    std::wstring res;
    for (const auto& s : args) {
        // Folosim funcția utilitară pentru a curăța fiecare argument
        res += stripQuotes(s);
    }
    return res;
}


std::wstring vShellEngine::fn_LEN(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"0";

    // Dacă argumentul este o variabilă (ex: LEN($a))
    if (args[0].size() > 0 && args[0][0] == L'$') {
        vDataValue val = getVarValue(args[0]);

        if (std::holds_alternative<vDataArray>(val)) {
            return std::to_wstring(std::get<vDataArray>(val).size());
        }
        if (std::holds_alternative<std::wstring>(val)) {
            return std::to_wstring(std::get<std::wstring>(val).size());
        }
        // Aici am înlocuit dataToString cu vDataToPrintable
        if (std::holds_alternative<long long>(val) || std::holds_alternative<double>(val)) {
            return std::to_wstring(vDataToPrintable(val).size());
        }

        // Dacă e monostate (variabilă inexistentă)
        if (std::holds_alternative<std::monostate>(val)) return L"0";
    }

    // Dacă este un literal string (ex: LEN("test"))
    std::wstring plainText = stripQuotes(args[0]);
    return std::to_wstring(plainText.size());
}

std::wstring vShellEngine::fn_GET_VAR_VAL(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"";

    // 1. Curățăm numele variabilei (în caz că vine cu ghilimele din CONCAT sau alte funcții)
    std::wstring varName = stripQuotes(args[0]);

    // 2. Ne asigurăm că are prefixul '$'
    if (varName.empty()) return L"";
    if (varName[0] != L'$') {
        varName = L"$" + varName;
    }

    // 3. Folosim metoda ta "motor" getVarValue
    vDataValue val = getVarValue(varName);

    // 4. Convertim rezultatul în string pentru a-l returna în fluxul de execuție
    // Folosim vDataToPrintable pentru a păstra formatul de array [1, 2] dacă e cazul
    return vDataToPrintable(val);
}

std::wstring vShellEngine::fn_SET_VAR_VAL(const std::vector<std::wstring>& args) {
    if (args.size() < 2) return L"Error: Missing arguments";

    std::wstring path = stripQuotes(args[0]);
    std::wstring newValueStr = args[1]; // Valoarea nouă (ca string/literal)

    if (path.empty()) return L"Error: Empty path";
    if (path[0] != L'$') path = L"$" + path;

    // 1. Dacă e o variabilă simplă (ex: $B)
    size_t bracketPos = path.find(L'[');
    if (bracketPos == std::wstring::npos) {
        // Folosim logica existentă de SET (presupunând că ai una)
        executeShellCommand(L"/set " + path + L"=" + newValueStr);
        return L"OK";
    }

    // 2. Dacă e indexare complexă (ex: $B[1][1])
    std::wstring varName = path.substr(0, bracketPos);
    auto it = m_variables.find(varName);
    if (it == m_variables.end()) return L"Error: Var not found";

    // Navigăm recursiv pentru a ajunge la referința obiectului
    vDataValue* currentVal = &(it->second.value);

    std::wstring indexPart = path.substr(bracketPos);
    size_t pos = 0;
    std::vector<int> indices;

    // Extragem toți indicii: [1][1] -> {1, 1}
    while ((pos = indexPart.find(L'[', pos)) != std::wstring::npos) {
        size_t endPos = indexPart.find(L']', pos);
        if (endPos == std::wstring::npos) break;
        indices.push_back(std::stoi(indexPart.substr(pos + 1, endPos - pos - 1)));
        pos = endPos + 1;
    }

    // Navigăm până la penultimul index
    for (size_t i = 0; i < indices.size(); ++i) {
        if (std::holds_alternative<vDataArray>(*currentVal)) {
            auto& arr = std::get<vDataArray>(*currentVal);
            int idx = indices[i];

            if (idx >= 0 && (size_t)idx < arr.size()) {
                if (i == indices.size() - 1) {
                    // AM AJUNS LA DESTINAȚIE: Modificăm valoarea
                    // Aici transformăm string-ul newValueStr în vDataValue (ex: resolveExpression)
                    arr[idx].value = parseLiteralToValue(newValueStr);
                }
                else {
                    currentVal = &(arr[idx].value);
                }
            }
            else return L"Error: Index out of bounds";
        }
        else return L"Error: Not an array";
    }

    return L"OK";
}

std::wstring vShellEngine::fn_PUSH(const std::vector<std::wstring>& args) {
    if (args.size() < 2) return L"Error: PUSH requires (array_path, value)";

    std::wstring path = stripQuotes(args[0]);
    if (path.empty()) return L"Error: Empty path";
    if (path[0] != L'$') path = L"$" + path;

    // 1. Găsim referința către obiectul pe care vrem să îl modificăm
    vDataValue* target = nullptr;

    // Folosim o logică similară cu SET_VAR_VAL pentru a naviga prin indici
    size_t bracketPos = path.find(L'[');
    std::wstring varName = (bracketPos == std::wstring::npos) ? path : path.substr(0, bracketPos);

    auto it = m_variables.find(varName);
    if (it == m_variables.end()) return L"Error: Variable not found";

    target = &(it->second.value);

    // Dacă avem indexare (ex: PUSH($B[0], "nou")), navigăm până la elementul final
    if (bracketPos != std::wstring::npos) {
        // ... (aici refolosești logica de navigare prin pointeri din SET_VAR_VAL) ...
        // Presupunem că am ajuns la target = pointer către elementul indexat
    }

    // 2. Verificăm dacă ținta este un Array
    if (std::holds_alternative<vDataArray>(*target)) {
        auto& arr = std::get<vDataArray>(*target);

        vData newValue;
        // Folosim parseLiteralToValue pentru a putea face PUSH(A, [1,2]) sau PUSH(A, $B)
        newValue.value = parseLiteralToValue(args[1]);

        arr.push_back(newValue);
        return L"OK";
    }

    return L"Error: Target is not an array";
}


std::wstring vShellEngine::fn_POP(const std::vector<std::wstring>& args) {
    if (args.empty()) return L"Error: POP requires (array)";

    std::wstring path = stripQuotes(args[0]);
    if (path[0] != L'$') path = L"$" + path;

    auto it = m_variables.find(path);
    if (it == m_variables.end()) return L"Error: Variable not found";

    if (std::holds_alternative<vDataArray>(it->second.value)) {
        auto& arr = std::get<vDataArray>(it->second.value);
        if (arr.empty()) return L"null";

        // 1. Salvăm valoarea ultimului element pentru a o returna
        vDataValue lastVal = arr.back().value;

        // 2. Îl ștergem din array
        arr.pop_back();

        // 3. Returnăm valoarea ștearsă (pentru a putea face /e $X = POP($List))
        return vDataToPrintable(lastVal);
    }
    return L"Error: Not an array";
}