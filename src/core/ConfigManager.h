#pragma once
#include <string>
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

private:
    std::unordered_set<std::string> m_hiddenEvents;
    bool m_showLocalTime = true;
    float m_windowAlpha = 0.88f;
};
