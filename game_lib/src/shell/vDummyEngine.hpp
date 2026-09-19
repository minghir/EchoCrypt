#ifndef VDUMMYENGINE_HPP
#define VDUMMYENGINE_HPP
#pragma once

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <cwctype>
#include <cctype>
#include <map>
#include <functional>
#include <variant>

#pragma once

#include "IShellEngine.hpp"
#include "../stringUtils.hpp"
#include "../ConsoleManager.hpp"
#include "vShellCommandParser.hpp"

class vDummyEngine : public IShellEngine {

private:
    bool m_running = true;
    std::wstring m_accumulator;
    std::vector<std::wstring> m_history;

public:

    vDummyEngine(){};

    std::wstring getPrompt() const override {
        return m_accumulator.empty() ? L"\ndummy# " : L"  -> ";
    }

    bool shouldExit() const override { return !m_running; }
    bool stop() { return m_running = false; }

    void execute(const std::wstring& line) {
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

        addToHistory(fullCommand);
        executeDummyCommand(fullCommand);
    }
    
    void addToHistory(const std::wstring& command) {
        m_history.push_back(command);
        // Salvare opțională în fișier
        std::wofstream historyFile("history.txt", std::ios::app);
        if (historyFile.is_open()) {
            historyFile << command << std::endl;
        }
    }

private:
    void executeDummyCommand(const std::wstring& command) {
        LOG_INFO(command);
    }

};
#endif