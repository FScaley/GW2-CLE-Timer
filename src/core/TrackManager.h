#pragma once
#include <string>
#include <vector>
#include <ctime>

class TimerEngine;
struct EventDef;
struct Segment;

struct TrackedEvent {
    std::string wikiKey;
    std::string segmentName;
    struct ReminderState {
        time_t lastFiredOccStart = 0;
    };
    ReminderState reminders[3];
};

struct Notification {
    std::string segmentName;
    std::string chatlink;
    int threshold;
    int minutesUntil;
    bool started;
};

struct TrackRow {
    const TrackedEvent* track;
    std::string displayName;
    std::string chatlink;
    int minutesUntilStart;
    bool isActive;
};

class TrackManager {
public:
    void AddTrack(const std::string& wikiKey, const std::string& segmentName,
                  const TimerEngine& engine, time_t utcNow);
    void RemoveTrack(const std::string& wikiKey, const std::string& segmentName);
    bool IsTracked(const std::string& wikiKey, const std::string& segmentName) const;
    bool IsEmpty() const { return m_tracked.empty(); }

    std::vector<Notification> Tick(const TimerEngine& engine, time_t utcNow);
    std::vector<TrackRow> GetTrackList(const TimerEngine& engine, int nowMin) const;

    std::vector<std::pair<std::string,std::string>> GetPairs() const;
    void LoadPairs(const std::vector<std::pair<std::string,std::string>>& pairs);

    static constexpr int THRESHOLDS[3] = {10, 2, 0};

private:
    time_t ComputeOccurrenceStart(const TimerEngine& engine, const TrackedEvent& te,
                                   time_t utcNow, int nowMin) const;
    int FindSegmentId(const TimerEngine& engine, const TrackedEvent& te) const;
    const EventDef* FindEventDef(const TimerEngine& engine, const std::string& wikiKey) const;

    std::vector<TrackedEvent> m_tracked;
};
