#include "GameEngine.hpp"
#include "stringUtils.hpp"
#include <sstream>

GameEngine::GameEngine(dbConnection& db) : m_db(db) {}

PlayerState GameEngine::getOrInitPlayerState(const std::wstring& username) {
    PlayerState state;
    state.username = username;

    // 1. Preluăm user_id
    std::wstring userQuery = L"SELECT id FROM echocrypt.users WHERE username = $1";
    if (m_db.execQuery(userQuery, { username }, "get_uid")) {
        if (m_db.fetchNextRow("get_uid")) {
            state.userId = std::stoi(m_db.fetchFieldByNumber(1, "get_uid"));
        }
        m_db.clearStatement("get_uid");
    }

    if (state.userId == 0) return state;

    // 2. Căutăm progresul în player_stats
    std::wstring statsQuery = L"SELECT level, hp, max_hp, last_x, last_y, current_dungeon_id FROM echocrypt.player_stats WHERE user_id = $1";
    if (m_db.execQuery(statsQuery, { std::to_wstring(state.userId) }, "get_stats")) {
        if (m_db.fetchNextRow("get_stats")) {
            state.level = std::stoi(m_db.fetchFieldByName(L"level", "get_stats"));
            state.hp = std::stoi(m_db.fetchFieldByName(L"hp", "get_stats"));
            state.maxHp = std::stoi(m_db.fetchFieldByName(L"max_hp", "get_stats"));
            state.x = std::stoi(m_db.fetchFieldByName(L"last_x", "get_stats"));
            state.y = std::stoi(m_db.fetchFieldByName(L"last_y", "get_stats"));

            std::wstring dId = m_db.fetchFieldByName(L"current_dungeon_id", "get_stats");
            state.dungeonId = dId.empty() ? 1 : std::stoi(dId);

            m_db.clearStatement("get_stats");
            return state;
        }
        m_db.clearStatement("get_stats");
    }

    // 3. Dacă e jucător nou, îl punem la coordonatele (0, 0) în dungeon 1
    std::wstring initQuery = L"INSERT INTO echocrypt.player_stats (user_id, level, hp, max_hp, last_x, last_y, current_dungeon_id) VALUES ($1, 1, 100, 100, 0, 0, 1)";
    m_db.execQuery(initQuery, { std::to_wstring(state.userId) }, "init_stats");
    m_db.clearStatement("init_stats");

    state.dungeonId = 1;
    state.x = 0;
    state.y = 0;
    return state;
}

bool GameEngine::updatePlayerPosition(int userId, int newX, int newY) {
    std::wstring updateQuery = L"UPDATE echocrypt.player_stats SET last_x = $1, last_y = $2 WHERE user_id = $3";
    bool ok = m_db.execQuery(updateQuery, { std::to_wstring(newX), std::to_wstring(newY), std::to_wstring(userId) }, "upd_pos");
    m_db.clearStatement("upd_pos");
    return ok;
}

std::wstring GameEngine::getRoomDescription(int dungeonId, int x, int y, std::wstring& outSymbol) {
    std::wstring query = L"SELECT base_description, ascii_symbol FROM echocrypt.rooms WHERE dungeon_id = $1 AND x = $2 AND y = $3";
    std::vector<std::wstring> params = { std::to_wstring(dungeonId), std::to_wstring(x), std::to_wstring(y) };

    std::wstring desc = L"Un perete masiv de piatră îți blochează drumul. Nu poți merge în această direcție.";
    outSymbol = L"#";

    if (m_db.execQuery(query, params, "get_room")) {
        if (m_db.fetchNextRow("get_room")) {
            desc = m_db.fetchFieldByName(L"base_description", "get_room");
            outSymbol = m_db.fetchFieldByName(L"ascii_symbol", "get_room");
        }
        m_db.clearStatement("get_room");
    }
    return desc;
}

std::wstring GameEngine::processRequest(const std::wstring& req, const std::wstring& user) {
    std::lock_guard<std::mutex> lock(m_engineMutex);

    PlayerState state = getOrInitPlayerState(user);
    std::wstring cmd = trim(req);

    int targetX = state.x;
    int targetY = state.y;
    bool isMoving = false;

    if (cmd == L"north" || cmd == L"n" || cmd == L"move north") { targetY += 1; isMoving = true; }
    else if (cmd == L"south" || cmd == L"s" || cmd == L"move south") { targetY -= 1; isMoving = true; }
    else if (cmd == L"east" || cmd == L"e" || cmd == L"move east") { targetX += 1; isMoving = true; }
    else if (cmd == L"west" || cmd == L"w" || cmd == L"west") { targetX -= 1; isMoving = true; }

    if (isMoving) {
        std::wstring symbol;
        std::wstring desc = getRoomDescription(state.dungeonId, targetX, targetY, symbol);

        // Dacă camera există (simbolul nu e perete '#')
        if (symbol != L"#") {
            state.x = targetX;
            state.y = targetY;
            updatePlayerPosition(state.userId, state.x, state.y);
        }

        std::wstringstream ss;
        ss << L"\n[ HP: " << state.hp << L"/" << state.maxHp << L" | Pozitia: (" << state.x << L"," << state.y << L") ]\n"
            << desc << L"\n";
        return ss.str();
    }

    if (cmd == L"look" || cmd == L"l") {
        std::wstring symbol;
        std::wstring desc = getRoomDescription(state.dungeonId, state.x, state.y, symbol);

        std::wstringstream ss;
        ss << L"\n[ HP: " << state.hp << L"/" << state.maxHp << L" | Pozitia: (" << state.x << L"," << state.y << L") ]\n"
            << desc << L"\n";
        return ss.str();
    }

    return L"Comanda invalida. Foloseste: n, s, e, w, look.";
}