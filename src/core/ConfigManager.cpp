#include "ConfigManager.h"
#include "../../include/json.hpp"
#include <fstream>

using json = nlohmann::json;

bool ConfigManager::Load(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    json root;
    try { root = json::parse(f); }
    catch (...) { return false; }

    m_hiddenEvents.clear();
    if (root.contains("hidden_events") && root["hidden_events"].is_array()) {
        for (auto& v : root["hidden_events"])
            if (v.is_string()) m_hiddenEvents.insert(v.get<std::string>());
    }

    m_showLocalTime = root.value("show_local_time", true);
    m_windowAlpha = root.value("window_alpha", 0.88f);
    m_remindMinutes = root.value("remind_minutes", 10);
    m_winX = root.value("win_x", -1.0f);
    m_winY = root.value("win_y", -1.0f);
    m_winW = root.value("win_w", -1.0f);
    m_winH = root.value("win_h", -1.0f);

    m_tracked.clear();
    if (root.contains("tracked") && root["tracked"].is_array()) {
        for (auto& entry : root["tracked"]) {
            if (entry.is_array() && entry.size() == 2
                && entry[0].is_string() && entry[1].is_string())
                m_tracked.push_back({entry[0].get<std::string>(), entry[1].get<std::string>()});
        }
    }

    return true;
}

bool ConfigManager::Save(const std::string& path) const {
    json root;
    json hidden = json::array();
    for (auto& k : m_hiddenEvents) hidden.push_back(k);
    root["hidden_events"] = hidden;
    root["show_local_time"] = m_showLocalTime;
    root["window_alpha"] = m_windowAlpha;
    root["remind_minutes"] = m_remindMinutes;
    root["win_x"] = m_winX;
    root["win_y"] = m_winY;
    root["win_w"] = m_winW;
    root["win_h"] = m_winH;

    json tracked = json::array();
    for (auto& [k, s] : m_tracked)
        tracked.push_back(json::array({k, s}));
    root["tracked"] = tracked;

    std::ofstream f(path);
    if (!f.is_open()) return false;
    f << root.dump(2);
    return true;
}

bool ConfigManager::IsEventVisible(const std::string& wikiKey) const {
    return m_hiddenEvents.find(wikiKey) == m_hiddenEvents.end();
}

void ConfigManager::SetEventVisible(const std::string& wikiKey, bool visible) {
    if (visible)
        m_hiddenEvents.erase(wikiKey);
    else
        m_hiddenEvents.insert(wikiKey);
}

void ConfigManager::SetDefaultVisibility(const std::string& wikiKey, bool defaultVisible) {
    if (!defaultVisible && m_hiddenEvents.find(wikiKey) == m_hiddenEvents.end()) {
        m_hiddenEvents.insert(wikiKey);
    }
}
