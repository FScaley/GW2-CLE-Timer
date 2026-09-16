#include <cstdio>
#include <ctime>
#include <string>
#include <fstream>
#include "core/TimerEngine.h"
#include "core/TrackManager.h"

static int g_pass = 0, g_fail = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("  FAIL: %s (line %d)\n", msg, __LINE__); g_fail++; } \
    else { g_pass++; } \
} while(0)

static time_t MakeUTC(int year, int month, int day, int hour, int min, int sec) {
    struct tm t = {};
    t.tm_year = year - 1900; t.tm_mon = month - 1; t.tm_mday = day;
    t.tm_hour = hour; t.tm_min = min; t.tm_sec = sec;
    return _mkgmtime(&t);
}

int main() {
    setvbuf(stdout, nullptr, _IONBF, 0);
    printf("=== CLE TrackManager Tests ===\n\n");

    TimerEngine engine;
    CHECK(engine.LoadFromFile("../data/events.json"), "engine loaded");

    // TEST 1: Add/Remove/IsTracked
    printf("TEST 1: Add/Remove/IsTracked\n");
    {
        TrackManager tm;
        CHECK(!tm.IsTracked("core-hwb", "Tequatl the Sunless"), "not tracked initially");
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, MakeUTC(2026,9,16,0,0,0));
        CHECK(tm.IsTracked("core-hwb", "Tequatl the Sunless"), "tracked after add");
        CHECK(!tm.IsTracked("core-hwb", "Karka Queen"), "other not tracked");
        tm.RemoveTrack("core-hwb", "Tequatl the Sunless");
        CHECK(!tm.IsTracked("core-hwb", "Tequatl the Sunless"), "not tracked after remove");
    }
    printf("\n");

    // TEST 2: Tequatl at 02:45 - should fire 10min reminder at 02:50
    printf("TEST 2: Tequatl 10min reminder\n");
    {
        TrackManager tm;
        // Add at 02:45 (Tequatl next at 03:00, 15 min away)
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, MakeUTC(2026,9,16,2,45,0));

        // Tick at 02:49 (11 min before) - no fire yet
        time_t t1 = MakeUTC(2026, 9, 16, 2, 49, 0);
        auto n1 = tm.Tick(engine, t1);
        CHECK(n1.empty(), "no notification at 02:49 (11min before)");

        // Tick at 02:50 (10 min before) - should fire [10]
        time_t t2 = MakeUTC(2026, 9, 16, 2, 50, 0);
        auto n2 = tm.Tick(engine, t2);
        CHECK(n2.size() == 1, "one notification at 02:50");
        if (!n2.empty()) {
            CHECK(n2[0].threshold == 10, "threshold is 10");
            CHECK(n2[0].segmentName == "Tequatl the Sunless", "correct segment");
            printf("  -> %s, %ddk threshold, %ddk kaldi\n",
                n2[0].segmentName.c_str(), n2[0].threshold, n2[0].minutesUntil);
        }

        // Tick at 02:51 - no re-fire
        time_t t3 = MakeUTC(2026, 9, 16, 2, 51, 0);
        auto n3 = tm.Tick(engine, t3);
        CHECK(n3.empty(), "no re-fire at 02:51");

        // Tick at 02:58 - should fire [2]
        time_t t4 = MakeUTC(2026, 9, 16, 2, 58, 0);
        auto n4 = tm.Tick(engine, t4);
        CHECK(n4.size() == 1, "one notification at 02:58");
        if (!n4.empty()) {
            CHECK(n4[0].threshold == 2, "threshold is 2");
        }

        // Tick at 03:00 - should fire [0] (started)
        time_t t5 = MakeUTC(2026, 9, 16, 3, 0, 0);
        auto n5 = tm.Tick(engine, t5);
        CHECK(!n5.empty(), "notification at 03:00");
        if (!n5.empty()) {
            CHECK(n5[0].threshold == 0, "threshold is 0");
            CHECK(n5[0].started, "marked as started");
        }

        // Tick at 03:01 - no more
        time_t t6 = MakeUTC(2026, 9, 16, 3, 1, 0);
        auto n6 = tm.Tick(engine, t6);
        CHECK(n6.empty(), "no notification at 03:01");
    }
    printf("\n");

    // TEST 3: No retroactive notification on add
    printf("TEST 3: No retroactive on add\n");
    {
        TrackManager tm;
        // Add at 02:55 (Tequatl at 03:00, 5 min away -- inside 10min window)
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, MakeUTC(2026,9,16,2,55,0));

        // Tick immediately -- [10] should NOT fire (suppressed)
        time_t t1 = MakeUTC(2026, 9, 16, 2, 55, 0);
        auto n1 = tm.Tick(engine, t1);
        CHECK(n1.empty(), "no retroactive [10] at 02:55");

        // [2] should fire at 02:58
        time_t t2 = MakeUTC(2026, 9, 16, 2, 58, 0);
        auto n2 = tm.Tick(engine, t2);
        CHECK(n2.size() == 1, "[2] fires at 02:58");
        if (!n2.empty()) CHECK(n2[0].threshold == 2, "threshold is 2");
    }
    printf("\n");

    // TEST 4: TrackList sorted
    printf("TEST 4: TrackList\n");
    {
        TrackManager tm;
        time_t t0 = MakeUTC(2026, 9, 16, 0, 0, 0);
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, t0);
        tm.AddTrack("core-hwb", "Triple Trouble", engine, t0);
        tm.AddTrack("core-hwb", "Karka Queen", engine, t0);

        auto list = tm.GetTrackList(engine, 0);
        CHECK(list.size() == 3, "3 tracked items");
        if (list.size() == 3) {
            // At 00:00: Tequatl active (0dk), TT at 01:00 (60dk), Karka at 02:00 (120dk)
            CHECK(list[0].isActive, "first is active");
            CHECK(list[0].displayName == "Tequatl the Sunless", "Tequatl first (active)");
            printf("  List: %s(%ddk) | %s(%ddk) | %s(%ddk)\n",
                list[0].displayName.c_str(), list[0].minutesUntilStart,
                list[1].displayName.c_str(), list[1].minutesUntilStart,
                list[2].displayName.c_str(), list[2].minutesUntilStart);
        }
    }
    printf("\n");

    // TEST 5: Persistence (GetPairs/LoadPairs)
    printf("TEST 5: Persistence\n");
    {
        TrackManager tm1;
        time_t t0 = MakeUTC(2026, 9, 16, 0, 0, 0);
        tm1.AddTrack("core-hwb", "Tequatl the Sunless", engine, t0);
        tm1.AddTrack("hot-ab", "Octovine", engine, t0);
        auto pairs = tm1.GetPairs();
        CHECK(pairs.size() == 2, "2 pairs exported");

        TrackManager tm2;
        tm2.LoadPairs(pairs);
        CHECK(tm2.IsTracked("core-hwb", "Tequatl the Sunless"), "Tequatl restored");
        CHECK(tm2.IsTracked("hot-ab", "Octovine"), "Octovine restored");
    }
    printf("\n");

    // TEST 6: LoadPairs cascade suppression
    printf("TEST 6: LoadPairs cascade suppression\n");
    {
        TrackManager tm;
        std::vector<std::pair<std::string,std::string>> pairs = {{"core-hwb", "Tequatl the Sunless"}};
        tm.LoadPairs(pairs);
        time_t t1 = MakeUTC(2026, 9, 16, 2, 55, 0);
        auto n1 = tm.Tick(engine, t1);
        CHECK(n1.size() <= 1, "at most 1 notification after LoadPairs (not 3 cascade)");
        if (!n1.empty()) printf("  -> threshold=%d\n", n1[0].threshold);
    }
    printf("\n");

    // TEST 7: Tick jump (02:45 -> 03:00)
    printf("TEST 7: Tick jump\n");
    {
        TrackManager tm;
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, MakeUTC(2026,9,16,2,30,0));
        time_t t1 = MakeUTC(2026, 9, 16, 2, 45, 0);
        tm.Tick(engine, t1);
        time_t t2 = MakeUTC(2026, 9, 16, 3, 0, 0);
        auto n2 = tm.Tick(engine, t2);
        CHECK(n2.size() == 1, "exactly 1 notification on jump");
        if (!n2.empty()) CHECK(n2[0].threshold == 0, "threshold 0");
    }
    printf("\n");

    // TEST 8: Custom 15dk threshold
    printf("TEST 8: Custom 15dk threshold\n");
    {
        TrackManager tm;
        tm.AddTrack("core-hwb", "Tequatl the Sunless", engine, MakeUTC(2026,9,16,2,30,0), 15);
        time_t t1 = MakeUTC(2026, 9, 16, 2, 48, 0);
        auto n1 = tm.Tick(engine, t1, 15);
        CHECK(n1.size() == 1, "fires within 15dk window");
        if (!n1.empty()) {
            CHECK(n1[0].threshold == 15, "threshold is 15");
            printf("  -> threshold=%d, minutesUntil=%d\n", n1[0].threshold, n1[0].minutesUntil);
        }
    }
    printf("\n");

    printf("=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
