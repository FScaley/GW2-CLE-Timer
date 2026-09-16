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

    bool GetSoundEnabled() const { return m_soundEnabled; }
    void SetSoundEnabled(bool v) { m_soundEnabled = v; }

    const std::vector<std::pair<std::string,std::string>>& GetTracked() const { return m_tracked; }
    void SetTracked(const std::vector<std::pair<std::string,std::string>>& t) { m_tracked = t; }

private:
    std::unordered_set<std::string> m_hiddenEvents;
    std::vector<std::pair<std::string,std::string>> m_tracked;
    bool m_showLocalTime = true;
    bool m_soundEnabled = true;
    float m_windowAlpha = 0.88f;
};
