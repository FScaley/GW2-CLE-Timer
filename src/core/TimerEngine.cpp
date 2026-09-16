#include "TimerEngine.h"
#include "../../include/json.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>

using json = nlohmann::json;

const TimerEngine::EventMapping TimerEngine::s_mappings[] = {
    // Core Tyria
    {"core-wb",     Expansion::Core,                EventCategory::WorldBoss,       true,  0, nullptr},
    {"core-hwb",    Expansion::Core,                EventCategory::WorldBoss,       true,  1, nullptr},
    {"core-la",     Expansion::Core,                EventCategory::LeyLineAnomaly,  true,  3, nullptr},
    {"core-in",     Expansion::Core,                EventCategory::MetaEvent,       false, 4, nullptr},
    {"lws2-dt",     Expansion::Core,                EventCategory::MetaEvent,       true,  2, nullptr},

    // Heart of Thorns + LW3
    {"hot-vb",      Expansion::HeartOfThorns,       EventCategory::MetaEvent,       true,  0, nullptr},
    {"hot-ab",      Expansion::HeartOfThorns,       EventCategory::MetaEvent,       true,  1, nullptr},
    {"hot-td",      Expansion::HeartOfThorns,       EventCategory::MetaEvent,       true,  2, nullptr},
    {"hot-ds",      Expansion::HeartOfThorns,       EventCategory::MetaEvent,       true,  3, nullptr},
    {"lws3-ld",     Expansion::HeartOfThorns,       EventCategory::MetaEvent,       false, 4, nullptr},

    // Path of Fire + LW4
    {"pof-co",      Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  0, nullptr},
    {"pof-dh",      Expansion::PathOfFire,          EventCategory::MetaEvent,       false, 1, nullptr},
    {"pof-er",      Expansion::PathOfFire,          EventCategory::MetaEvent,       false, 2, nullptr},
    {"pof-td",      Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  3, nullptr},
    {"pof-dv",      Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  4, nullptr},
    {"lws4-di",     Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  5, nullptr},
    {"lws4-jb",     Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  6, nullptr},
    {"lws4-tp",     Expansion::PathOfFire,          EventCategory::MetaEvent,       true,  7, nullptr},

    // Icebrood Saga
    {"lws5-bm",     Expansion::IcebroodSaga,        EventCategory::MetaEvent,       true,  0, nullptr},
    {"lws5-gv",     Expansion::IcebroodSaga,        EventCategory::MetaEvent,       false, 1, nullptr},

    // End of Dragons
    {"eod-sp",      Expansion::EndOfDragons,        EventCategory::MetaEvent,       true,  0, nullptr},
    {"eod-nkc",     Expansion::EndOfDragons,        EventCategory::MetaEvent,       true,  1, nullptr},
    {"eod-ew",      Expansion::EndOfDragons,        EventCategory::MetaEvent,       true,  2, nullptr},
    {"eod-de",      Expansion::EndOfDragons,        EventCategory::MetaEvent,       true,  3, nullptr},

    // Secrets of the Obscure
    {"soto-sa",     Expansion::SecretsOfTheObscure, EventCategory::MetaEvent,       true,  0, nullptr},
    {"soto-am",     Expansion::SecretsOfTheObscure, EventCategory::MetaEvent,       true,  1, nullptr},

    // Janthir Wilds
    {"jw-js",       Expansion::JanthirWilds,        EventCategory::MetaEvent,       true,  0, nullptr},
    {"jw-bn",       Expansion::JanthirWilds,        EventCategory::MetaEvent,       true,  1, nullptr},

    // Visions of Eternity
    {"voe-ss",      Expansion::VisionsOfEternity,   EventCategory::MetaEvent,       true,  0, nullptr},
    {"voe-sw",      Expansion::VisionsOfEternity,   EventCategory::MetaEvent,       true,  1, nullptr},
    {"voe-eg",      Expansion::VisionsOfEternity,   EventCategory::MetaEvent,       true,  2, nullptr},

    // Convergence -- split by segment into correct expansions
    {"public-con",  Expansion::SecretsOfTheObscure, EventCategory::Convergence,     true,  2, "Outer Nayos"},
    {"public-con",  Expansion::JanthirWilds,        EventCategory::Convergence,     true,  2, "Mount Balrior"},

    // Dragonstorm only (not Marionette/BoLA/ToN)
    {"public-eotn", Expansion::IcebroodSaga,        EventCategory::Dragonstorm,     true,  2, "Dragonstorm"},
};
const int TimerEngine::s_mappingCount = sizeof(s_mappings) / sizeof(s_mappings[0]);

const char* ExpansionName(Expansion e) {
    switch (e) {
        case Expansion::Core:                return "Core Tyria";
        case Expansion::HeartOfThorns:       return "Heart of Thorns";
        case Expansion::PathOfFire:          return "Path of Fire";
        case Expansion::IcebroodSaga:        return "Icebrood Saga";
        case Expansion::EndOfDragons:        return "End of Dragons";
        case Expansion::SecretsOfTheObscure: return "Secrets of the Obscure";
        case Expansion::JanthirWilds:        return "Janthir Wilds";
        case Expansion::VisionsOfEternity:   return "Visions of Eternity";
        default: return "Unknown";
    }
}

const char* CategoryName(EventCategory c) {
    switch (c) {
        case EventCategory::WorldBoss:      return "World Boss";
        case EventCategory::MetaEvent:      return "Meta Event";
        case EventCategory::Convergence:    return "Convergence";
        case EventCategory::LeyLineAnomaly: return "Ley-Line Anomaly";
        case EventCategory::Dragonstorm:    return "Dragonstorm";
        default: return "Unknown";
    }
}

bool TimerEngine::ParseJson(const std::string& jsonStr) {
    json root;
    try { root = json::parse(jsonStr); }
    catch (...) { return false; }
    if (!root.contains("events")) return false;
    const auto& events = root["events"];

    m_events.clear();
    for (int mi = 0; mi < s_mappingCount; ++mi) {
        const auto& map = s_mappings[mi];
        if (!events.contains(map.wikiKey)) continue;

        const auto& ev = events[map.wikiKey];
        EventDef def;
        def.wikiKey = map.wikiKey;
        def.name = ev.value("name", map.wikiKey);
        def.expansion = map.expansion;
        def.category = map.category;
        def.defaultVisible = map.defaultVisible;
        def.displayOrder = map.displayOrder;
        def.segmentFilter = map.segmentFilter;

        if (ev.contains("segments")) {
            for (auto& [key, seg] : ev["segments"].items()) {
                Segment s;
                s.id = std::stoi(key);
                s.name = seg.value("name", "");
                s.chatlink = seg.value("chatlink", "");
                s.isGap = s.name.empty();
                def.segments.push_back(s);
            }
        }

        auto parseSeq = [](const json& arr) -> std::vector<SeqEntry> {
            std::vector<SeqEntry> result;
            for (auto& entry : arr) {
                SeqEntry se;
                se.segRef = entry.value("r", 0);
                se.durationMin = entry.value("d", 0);
                result.push_back(se);
            }
            return result;
        };

        if (ev.contains("sequences")) {
            const auto& seq = ev["sequences"];
            if (seq.contains("partial"))
                def.partial = parseSeq(seq["partial"]);
            if (seq.contains("pattern"))
                def.pattern = parseSeq(seq["pattern"]);
        }

        def.partialTotalMin = 0;
        for (auto& e : def.partial) def.partialTotalMin += e.durationMin;
        def.patternTotalMin = 0;
        for (auto& e : def.pattern) def.patternTotalMin += e.durationMin;

        m_events.push_back(std::move(def));
    }

    BuildGroups();
    return !m_events.empty();
}

bool TimerEngine::LoadFromFile(const std::string& jsonPath) {
    std::ifstream f(jsonPath);
    if (!f.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return ParseJson(content);
}

bool TimerEngine::LoadFromMemory(const void* data, size_t size) {
    std::string content(reinterpret_cast<const char*>(data), size);
    return ParseJson(content);
}

const Segment* TimerEngine::FindSegment(const EventDef& ev, int segRef) const {
    for (auto& s : ev.segments)
        if (s.id == segRef) return &s;
    return nullptr;
}

TimerEngine::PhaseInfo TimerEngine::GetPhaseAt(const EventDef& ev, int minutesSinceMidnight) const {
    PhaseInfo info{};
    int pos = minutesSinceMidnight % 1440;

    int elapsed = 0;
    for (auto& entry : ev.partial) {
        if (pos < elapsed + entry.durationMin) {
            info.segment = FindSegment(ev, entry.segRef);
            info.elapsedInPhase = pos - elapsed;
            info.phaseDuration = entry.durationMin;
            info.minutesUntilEnd = entry.durationMin - (pos - elapsed);
            return info;
        }
        elapsed += entry.durationMin;
    }

    if (ev.patternTotalMin <= 0) return info;

    int remaining = pos - elapsed;
    if (remaining < 0) remaining += 1440;
    int posInPattern = remaining % ev.patternTotalMin;

    for (auto& entry : ev.pattern) {
        if (posInPattern < entry.durationMin) {
            info.segment = FindSegment(ev, entry.segRef);
            info.elapsedInPhase = posInPattern;
            info.phaseDuration = entry.durationMin;
            info.minutesUntilEnd = entry.durationMin - posInPattern;
            return info;
        }
        posInPattern -= entry.durationMin;
    }

    return info;
}

int TimerEngine::MinutesUntilSegment(const EventDef& ev, int segId, int nowMin) const {
    // Check if currently in this segment
    PhaseInfo cur = GetPhaseAt(ev, nowMin);
    if (cur.segment && cur.segment->id == segId)
        return 0;

    // Scan forward up to 1440 minutes
    int elapsed = 0;
    int checkMin = nowMin;
    for (int offset = 1; offset <= 1440; ++offset) {
        checkMin = (nowMin + offset) % 1440;
        PhaseInfo pi = GetPhaseAt(ev, checkMin);
        if (pi.segment && pi.segment->id == segId && pi.elapsedInPhase == 0)
            return offset;
    }
    return 9999;
}

static std::string FormatCountdown(int totalMinutes) {
    if (totalMinutes <= 0) return "AKTIF";
    int h = totalMinutes / 60;
    int m = totalMinutes % 60;
    char buf[32];
    if (h > 0)
        snprintf(buf, sizeof(buf), "%ds %02dd", h, m);
    else
        snprintf(buf, sizeof(buf), "%dd", m);
    return buf;
}

void TimerEngine::GenerateBossRows(const EventDef& ev, int nowMin, std::vector<EventRow>& out) {
    // One row per distinct non-gap segment, sorted by soonest
    std::vector<EventRow> rows;
    for (auto& seg : ev.segments) {
        if (seg.isGap) continue;
        if (ev.segmentFilter && seg.name != ev.segmentFilter) continue;

        EventRow row{};
        row.def = &ev;
        row.displayName = seg.name;
        row.chatlink = seg.chatlink;

        PhaseInfo cur = GetPhaseAt(ev, nowMin);
        if (cur.segment && cur.segment->id == seg.id) {
            row.isActive = true;
            row.minutesUntilStart = 0;
            row.minutesRemaining = cur.minutesUntilEnd;
            row.countdown = "AKTIF - " + std::to_string(cur.minutesUntilEnd) + "d";
        } else {
            row.isActive = false;
            row.minutesUntilStart = MinutesUntilSegment(ev, seg.id, nowMin);
            row.minutesRemaining = 0;
            row.countdown = FormatCountdown(row.minutesUntilStart);
        }
        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(), [](const EventRow& a, const EventRow& b) {
        if (a.isActive != b.isActive) return a.isActive;
        return a.minutesUntilStart < b.minutesUntilStart;
    });

    for (auto& r : rows) out.push_back(std::move(r));
}

void TimerEngine::GenerateMetaRow(const EventDef& ev, int nowMin, std::vector<EventRow>& out) {
    PhaseInfo cur = GetPhaseAt(ev, nowMin);
    if (!cur.segment) return;

    EventRow row{};
    row.def = &ev;
    row.chatlink = cur.segment->chatlink;
    row.isActive = !cur.segment->isGap;

    // Show: MapName: CurrentPhase → next phase change in Xm
    if (cur.segment->isGap) {
        row.displayName = ev.name;
        row.minutesUntilStart = cur.minutesUntilEnd;
        row.countdown = FormatCountdown(cur.minutesUntilEnd);
    } else {
        row.displayName = ev.name + ": " + cur.segment->name;
        row.minutesUntilStart = 0;
        row.minutesRemaining = cur.minutesUntilEnd;
        row.countdown = cur.minutesUntilEnd <= 0 ? "AKTIF" :
            (std::to_string(cur.minutesUntilEnd) + "d kaldi");
    }

    out.push_back(std::move(row));
}

void TimerEngine::GenerateRows(int minutesSinceMidnight) {
    for (auto& group : m_groups)
        group.rows.clear();

    for (auto& ev : m_events) {
        int expIdx = static_cast<int>(ev.expansion);
        if (expIdx < 0 || expIdx >= static_cast<int>(m_groups.size())) continue;

        auto& rows = m_groups[expIdx].rows;
        switch (ev.category) {
            case EventCategory::WorldBoss:
            case EventCategory::LeyLineAnomaly:
            case EventCategory::Convergence:
            case EventCategory::Dragonstorm:
                GenerateBossRows(ev, minutesSinceMidnight, rows);
                break;
            case EventCategory::MetaEvent:
                GenerateMetaRow(ev, minutesSinceMidnight, rows);
                break;
            default:
                break;
        }
    }

    for (auto& group : m_groups) {
        std::stable_sort(group.rows.begin(), group.rows.end(),
            [](const EventRow& a, const EventRow& b) {
                if (a.def->category != b.def->category)
                    return static_cast<int>(a.def->category) < static_cast<int>(b.def->category);
                if (a.def->displayOrder != b.def->displayOrder)
                    return a.def->displayOrder < b.def->displayOrder;
                return false;
            });
    }
}

void TimerEngine::Update(time_t utcNow) {
    struct tm utcTm;
#ifdef _WIN32
    gmtime_s(&utcTm, &utcNow);
#else
    gmtime_r(&utcNow, &utcTm);
#endif
    int minutesSinceMidnight = utcTm.tm_hour * 60 + utcTm.tm_min;
    GenerateRows(minutesSinceMidnight);
}

void TimerEngine::BuildGroups() {
    m_groups.clear();
    m_groups.resize(static_cast<size_t>(Expansion::COUNT));
    for (int i = 0; i < static_cast<int>(Expansion::COUNT); ++i) {
        m_groups[i].expansion = static_cast<Expansion>(i);
        m_groups[i].name = ExpansionName(static_cast<Expansion>(i));
    }
}
