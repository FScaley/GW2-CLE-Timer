#pragma once
#include <string>
#include <vector>
#include <ctime>

enum class Expansion : uint8_t {
    Core = 0,
    HeartOfThorns,
    PathOfFire,
    IcebroodSaga,
    EndOfDragons,
    SecretsOfTheObscure,
    JanthirWilds,
    VisionsOfEternity,
    COUNT
};

enum class EventCategory : uint8_t {
    WorldBoss = 0,
    MetaEvent,
    Convergence,
    LeyLineAnomaly,
    Dragonstorm,
    COUNT
};

struct SegColor {
    uint8_t r = 100, g = 100, b = 100;
};

struct Segment {
    int id;
    std::string name;
    std::string chatlink;
    bool isGap;
    SegColor color;
};

struct SeqEntry {
    int segRef;
    int durationMin;
};

struct EventDef {
    std::string wikiKey;
    std::string name;
    std::string displayName;
    Expansion expansion;
    EventCategory category;
    bool defaultVisible;
    int displayOrder;
    const char* segmentFilter;
    std::vector<Segment> segments;
    std::vector<SeqEntry> partial;
    std::vector<SeqEntry> pattern;
    int partialTotalMin;
    int patternTotalMin;
};

struct EventRow {
    const EventDef* def;
    std::string displayName;
    std::string chatlink;
    bool isActive;
    int minutesUntilStart;
    int minutesRemaining;
    std::string countdown;
};

struct ExpansionGroup {
    Expansion expansion;
    std::string name;
    std::vector<EventRow> rows;
};

const char* ExpansionName(Expansion e);
const char* CategoryName(EventCategory c);

class TimerEngine {
public:
    bool LoadFromFile(const std::string& jsonPath);
    bool LoadFromMemory(const void* data, size_t size);
    void Update(time_t utcNow);

    const std::vector<ExpansionGroup>& GetGroups() const { return m_groups; }
    const std::vector<EventDef>& GetAllEvents() const { return m_events; }

    int MinutesUntilSegment(const EventDef& ev, int segId, int nowMin) const;

    struct PhaseInfo {
        const Segment* segment = nullptr;
        int elapsedInPhase = 0;
        int phaseDuration = 0;
        int minutesUntilEnd = 0;
    };
    PhaseInfo GetPhaseAt(const EventDef& ev, int minutesSinceMidnight) const;

private:
    bool ParseJson(const std::string& jsonStr);
    void BuildGroups();
    void GenerateRows(int minutesSinceMidnight);
    void GenerateBossRows(const EventDef& ev, int nowMin, std::vector<EventRow>& out);
    void GenerateMetaRow(const EventDef& ev, int nowMin, std::vector<EventRow>& out);
    const Segment* FindSegment(const EventDef& ev, int segRef) const;

    std::vector<EventDef> m_events;
    std::vector<ExpansionGroup> m_groups;

    struct EventMapping {
        const char* wikiKey;
        Expansion expansion;
        EventCategory category;
        bool defaultVisible;
        int displayOrder;
        const char* segmentFilter;
        const char* displayNameOverride;
    };
    static const EventMapping s_mappings[];
    static const int s_mappingCount;
};
