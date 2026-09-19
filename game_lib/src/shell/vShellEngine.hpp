#ifndef VDBFSHELLENGINE_HPP
#define VDBFSHELLENGINE_HPP
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
#include "../db/dbConnection.hpp"
#include "IShellEngine.hpp"
#include "vShellCommandParser.hpp"




#include <vector>
#include <memory>

struct CommandBlock {
    std::wstring cmd;
    std::wstring op; // "", "&&", "||"
};



struct vData; // Forward declaration

using vDataArray = std::vector<vData>;
using vDataMap = std::map<std::wstring, vData>;
//using vDataValue = std::variant<std::monostate, std::wstring, vDataArray>;

using vDataValue = std::variant<
    std::monostate,
    std::wstring,
    long long,
    double,
    bool,
    vDataArray,
    vDataMap
>;


struct vData {
    vDataValue value;

    // Utilitar pentru a verifica dacă este array sau string
    bool isArray() const { return std::holds_alternative<vDataArray>(value); }
    bool isMap() const { return std::holds_alternative<vDataMap>(value); }
    
    bool isString() const { return std::holds_alternative<std::wstring>(value); }
};

using vValue = std::variant<std::monostate, std::wstring, int, double, long long, bool>;




class vShellEngine : public IShellEngine {

private:
//    IQueryProvider& m_provider;
    bool m_running;
    std::wstring m_accumulator;
    std::vector<std::wstring> m_history;
    //std::map<std::wstring, std::wstring> m_variables;
    std::map<std::wstring, vData> m_variables;

    using ShellHandler = std::function<bool(const ShellCommand&)>;
    std::map<std::wstring, ShellHandler> m_commandHandlers;
    

    using ShellFunction = std::function<std::wstring(const std::vector<std::wstring>&)>;
    std::map<std::wstring, ShellFunction> m_functionHandlers;

    std::unique_ptr<dbConnection> dbfConn;


public:
    

    //vShellEngine(IQueryProvider& provider);
    vShellEngine();

    std::wstring getPrompt() const override {
        return m_accumulator.empty() ? L"\ndbf# " : L"  -> ";
    }

    bool shouldExit() const override { return !m_running; }
    bool stop() { return m_running = false; }

    void execute(const std::wstring& line);

    void processQuery(std::wstring query);

    std::vector<std::wstring> splitShellCommands(const std::wstring& line);
    std::vector<CommandBlock> splitByLogicalOperators(const std::wstring& line);
    bool executeLogicalLine(const std::wstring& line);
    bool executeShellCommand(const std::wstring& line);
    
    void initializeCommandsHandlers();
    void initializeFunctionsHandlers();

   // std::wstring vValueToString(const vValue& val);
    std::wstring vValueToString(const vDataValue& val);

    std::vector<std::wstring> splitArguments(const std::wstring& s);
    std::wstring processArgument(std::wstring arg);


    


    void handleInput(const std::wstring& line, std::wstring& accumulator);
    std::wstring normalizeVarName(std::wstring name);
    vDataValue getVarValue(std::wstring expression);
    bool displayVariables();
    bool displaySingleVariable(const std::wstring& varName);
    vData resolveExpression(std::wstring input);
    vData parseLiteral(const std::wstring& input, size_t& pos);

    void addToHistory(const std::wstring& query);
   
   std::wstring substituteVariables(std::wstring query, const std::map<std::wstring, vData>& vars);

   
     

   std::wstring formatPrintR(const vDataValue& data, int indent);
   std::wstring vDataToPrintable(const vDataValue& data);
   bool showHelp();
  
private:
    std::wstring stripQuotes(std::wstring s);
    vDataValue parseLiteralToValue(std::wstring str);


    ShellCommand processSetArgs(ShellCommand sc);

    vDataValue resolveVariableToValue(std::wstring input, const std::map<std::wstring, vData>& vars);


    bool executeScript(const std::wstring& filePath);

    //command handlers
    bool handleSetCommand(const ShellCommand& sc);
    bool handleUnsetCommand(const ShellCommand& sc);
    bool handleUnsetAllCommand(const ShellCommand& sc);
    bool handleEchoCommand(const ShellCommand& sc);
    bool handleImportCommand(const ShellCommand& sc);
    bool handleClearCommand(const ShellCommand& sc);
    bool handleHistoryCommand(const ShellCommand& sc);
    bool handleRunCommand(const ShellCommand& sc);
    bool handlePauseCommand(const ShellCommand& sc);
    bool handleForeachCommand(const ShellCommand& sc);
    bool handleIfCommand(const ShellCommand& sc);
    bool handleSysCommand(const ShellCommand& sc);
    bool handleQuitCommand(const ShellCommand& sc);

    bool handleEvalCommand(const ShellCommand& sc);
    bool handlePrintRCommand(const ShellCommand& sc);

    // Function Handlers
    std::wstring fn_SUM(const std::vector<std::wstring>& args);
    std::wstring fn_CONCAT(const std::vector<std::wstring>& args);
    std::wstring fn_UPPER(const std::vector<std::wstring>& args);
    std::wstring fn_LEN(const std::vector<std::wstring>& args);

    std::wstring fn_GET_VAR_VAL(const std::vector<std::wstring>& args);
    std::wstring fn_SET_VAR_VAL(const std::vector<std::wstring>& args);

    std::wstring fn_PUSH(const std::vector<std::wstring>& args);
    std::wstring fn_POP(const std::vector<std::wstring>& args);

    std::wstring fn_TYPEOF(const std::vector<std::wstring>& args);

};
#endif