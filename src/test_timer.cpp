#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <fstream>
#include "core/TimerEngine.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        printf("  FAIL: %s (line %d)\n", msg, __LINE__); \
        g_fail++; \
    } else { \
        g_pass++; \
    } \
} while(0)

static time_t MakeUTC(int year, int month, int day, int hour, int min, int sec) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min = min;
    t.tm_sec = sec;
    return _mkgmtime(&t);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== CLE Timer Engine Tests ===\n\n");

    TimerEngine engine;
    std::string dataPath = "../data/events.json";

    // TEST 1: Load
    printf("TEST 1: Load event data\n");
    CHECK(engine.LoadFromFile(dataPath), "events.json loaded");
    auto& allEvents = engine.GetAllEvents();
    printf("  Loaded %zu event defs\n", allEvents.size());
    CHECK(allEvents.size() >= 30, ">=30 events loaded");
    printf("\n");

    // TEST 2: World Boss at 00:00 -- per-segment row check
    printf("TEST 2: World Boss per-segment rows at 00:00 UTC\n");
    const EventDef* coreWB = nullptr;
    for (auto& ev : allEvents)
        if (ev.wikiKey == "core-wb") { coreWB = &ev; break; }
    CHECK(coreWB != nullptr, "core-wb found");
    if (coreWB) {
        auto pi = engine.GetPhaseAt(*coreWB, 0);
        CHECK(pi.segment && pi.segment->name == "Admiral Taidha Covington", "00:00 = Taidha");

        int toSvanir = engine.MinutesUntilSegment(*coreWB, 2, 0); // seg 2 = Svanir
        printf("  Taidha at 00:00, Svanir in %d min\n", toSvanir);
        CHECK(toSvanir == 15, "Svanir in 15m from 00:00");

        // Find Shatterer (seg 5) -- should be at 01:00 = 60 min
        int toShatterer = engine.MinutesUntilSegment(*coreWB, 5, 0);
        printf("  Shatterer in %d min from 00:00\n", toShatterer);
        CHECK(toShatterer == 60, "Shatterer in 60m from 00:00");
    }
    printf("\n");

    // TEST 3: Hard World Boss per-segment
    printf("TEST 3: Hard World Boss from 00:30 (gap)\n");
    const EventDef* hwb = nullptr;
    for (auto& ev : allEvents)
        if (ev.wikiKey == "core-hwb") { hwb = &ev; break; }
    CHECK(hwb != nullptr, "core-hwb found");
    if (hwb) {
        auto pi = engine.GetPhaseAt(*hwb, 30);
        CHECK(pi.segment && pi.segment->isGap, "00:30 = gap");

        // Tequatl (seg 3) next at 03:00 = 180m from 00:00, so from 00:30 = 150m
        int toTequatl = engine.MinutesUntilSegment(*hwb, 3, 30);
        printf("  Tequatl from 00:30: %d min\n", toTequatl);
        CHECK(toTequatl == 150, "Tequatl in 150m from 00:30");
    }
    printf("\n");

    // TEST 4: Ley-Line Anomaly per-segment
    printf("TEST 4: Ley-Line per-segment from 00:00\n");
    const EventDef* lla = nullptr;
    for (auto& ev : allEvents)
        if (ev.wikiKey == "core-la") { lla = &ev; break; }
    CHECK(lla != nullptr, "core-la found");
    if (lla) {
        int toTimberline = engine.MinutesUntilSegment(*lla, 1, 0); // seg 1 = Timberline
        int toIron = engine.MinutesUntilSegment(*lla, 2, 0);       // seg 2 = Iron Marches
        int toGendarran = engine.MinutesUntilSegment(*lla, 3, 0);   // seg 3 = Gendarran
        printf("  Timberline: %dm, Iron Marches: %dm, Gendarran: %dm\n",
            toTimberline, toIron, toGendarran);
        CHECK(toTimberline == 20, "Timberline in 20m from 00:00");
        CHECK(toIron == 140, "Iron Marches in 140m from 00:00");
        CHECK(toGendarran == 260, "Gendarran in 260m from 00:00");
    }
    printf("\n");

    // TEST 5: Auric Basin meta row
    printf("TEST 5: Auric Basin meta phases\n");
    const EventDef* ab = nullptr;
    for (auto& ev : allEvents)
        if (ev.wikiKey == "hot-ab") { ab = &ev; break; }
    CHECK(ab != nullptr, "hot-ab found");
    if (ab) {
        // At 00:00 UTC, AB should be in its partial (90m: Challenges 45m + Octovine 15m + Reset 30m)
        auto pi = engine.GetPhaseAt(*ab, 0);
        CHECK(pi.segment != nullptr, "phase at 00:00");
        if (pi.segment) printf("  AB at 00:00: %s (%dm left)\n", pi.segment->name.c_str(), pi.minutesUntilEnd);

        // Octovine check
        bool foundOctovine = false;
        for (int m = 0; m < 240; m += 1) {
            auto p = engine.GetPhaseAt(*ab, m);
            if (p.segment && p.segment->name == "Octovine" && p.elapsedInPhase == 0) {
                foundOctovine = true;
                printf("  Octovine starts at %02d:%02d\n", m / 60, m % 60);
                break;
            }
        }
        CHECK(foundOctovine, "Octovine found in AB cycle");
    }
    printf("\n");

    // TEST 6: Full Update -- rows generated
    printf("TEST 6: Full Update at 2026-09-16 00:00 UTC\n");
    time_t testTime = MakeUTC(2026, 9, 16, 0, 0, 0);
    engine.Update(testTime);
    auto& groups = engine.GetGroups();

    int totalRows = 0;
    for (auto& g : groups)
        totalRows += static_cast<int>(g.rows.size());
    printf("  Total rows: %d\n", totalRows);
    CHECK(totalRows >= 30, ">=30 rows generated");

    // Core should have multiple world boss rows (not just 2)
    int coreWBRows = 0;
    for (auto& row : groups[0].rows)
        if (row.def->category == EventCategory::WorldBoss) coreWBRows++;
    printf("  Core World Boss rows: %d\n", coreWBRows);
    CHECK(coreWBRows >= 10, ">=10 world boss rows in Core (individual bosses)");
    printf("\n");

    // TEST 7: Convergence segment filtering
    printf("TEST 7: Convergence segment filtering\n");
    bool outerNayosInSotO = false;
    bool mountBalriorInJW = false;
    for (auto& row : groups[static_cast<int>(Expansion::SecretsOfTheObscure)].rows) {
        if (row.def->category == EventCategory::Convergence && row.displayName == "Outer Nayos")
            outerNayosInSotO = true;
    }
    for (auto& row : groups[static_cast<int>(Expansion::JanthirWilds)].rows) {
        if (row.def->category == EventCategory::Convergence && row.displayName == "Mount Balrior")
            mountBalriorInJW = true;
    }
    CHECK(outerNayosInSotO, "Outer Nayos convergence in SotO group");
    CHECK(mountBalriorInJW, "Mount Balrior convergence in JW group");
    printf("\n");

    // TEST 8: Dragonstorm filtering (only Dragonstorm, not Marionette/BoLA)
    printf("TEST 8: Dragonstorm filtering\n");
    bool hasDragonstormRow = false;
    bool hasMarionette = false;
    for (auto& row : groups[static_cast<int>(Expansion::IcebroodSaga)].rows) {
        if (row.displayName == "Dragonstorm") hasDragonstormRow = true;
        if (row.displayName == "Twisted Marionette") hasMarionette = true;
    }
    CHECK(hasDragonstormRow, "Dragonstorm row present in IBS");
    CHECK(!hasMarionette, "Marionette NOT in IBS (filtered out)");
    printf("\n");

    // TEST 9: Day-wrap for core-hwb (partial > 1440)
    printf("TEST 9: Day-wrap core-hwb\n");
    if (hwb) {
        auto piEnd = engine.GetPhaseAt(*hwb, 1439);
        CHECK(piEnd.segment != nullptr, "1439 min has a segment");
        if (piEnd.segment) printf("  At 23:59: %s (gap=%d)\n", piEnd.segment->name.c_str(), piEnd.segment->isGap);

        auto piWrap = engine.GetPhaseAt(*hwb, 0);
        auto piMod = engine.GetPhaseAt(*hwb, 1440);
        CHECK(piWrap.segment && piMod.segment, "both have segments");
        if (piWrap.segment && piMod.segment) {
            CHECK(piWrap.segment->id == piMod.segment->id, "1440 wraps to 0 (same segment)");
            CHECK(piWrap.elapsedInPhase == piMod.elapsedInPhase, "same elapsed in phase");
        }
    }
    printf("\n");

    // TEST 10: LoadFromMemory
    printf("TEST 10: LoadFromMemory\n");
    {
        TimerEngine engine2;
        std::string raw;
        {
            std::ifstream f(dataPath);
            raw.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
        }
        CHECK(engine2.LoadFromMemory(raw.data(), raw.size()), "LoadFromMemory succeeds");
        CHECK(engine2.GetAllEvents().size() >= 30, "same event count from memory");
    }
    printf("\n");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
