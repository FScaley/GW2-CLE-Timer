#pragma once
#include <string>
#include <vector>
#include <unordered_set>

class ConfigManager {
public:
    bool Load(const std::string& path);
    bool Save(const std::string& path) const;

    bool IsEventVisible(const std::string& wikiKey) const;
    void SetEventVisible(const std::string& wikiKey, bool visible);
    void SetDefaultVisibility(const std::string& wikiKey, bool defaultVisible);

    bool GetShowLocalTime() const { return m_showLocalTime; }
    void SetShowLocalTime(bool v) { m_showLocalTime = v; }

    float GetWindowAlpha() const { return m_windowAlpha; }
    void SetWindowAlpha(float v) { m_windowAlpha = v; }

    float GetWinX() const { return m_winX; }
    float GetWinY() const { return m_winY; }
    float GetWinW() const { return m_winW; }
    float GetWinH() const { return m_winH; }
    void SetWindowRect(float x, float y, float w, float h) { m_winX=x; m_winY=y; m_winW=w; m_winH=h; }

    int GetRemindMinutes() const { return m_remindMinutes; }
    void SetRemindMinutes(int v) { m_remindMinutes = v; }

    const std::vector<std::pair<std::string,std::string>>& GetTracked() const { return m_tracked; }
    void SetTracked(const std::vector<std::pair<std::string,std::string>>& t) { m_tracked = t; }

private:
    std::unordered_set<std::string> m_hiddenEvents;
    std::vector<std::pair<std::string,std::string>> m_tracked;
    bool m_showLocalTime = true;
    int m_remindMinutes = 10;
    float m_windowAlpha = 0.88f;
    float m_winX = -1, m_winY = -1, m_winW = -1, m_winH = -1;
};
