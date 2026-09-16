#include "TrackManager.h"
#include "TimerEngine.h"
#include <algorithm>

const EventDef* TrackManager::FindEventDef(const TimerEngine& engine, const std::string& wikiKey) const {
    for (auto& ev : engine.GetAllEvents())
        if (ev.wikiKey == wikiKey) return &ev;
    return nullptr;
}

int TrackManager::FindSegmentId(const TimerEngine& engine, const TrackedEvent& te) const {
    auto* ev = FindEventDef(engine, te.wikiKey);
    if (!ev) return -1;
    for (auto& seg : ev->segments)
        if (seg.name == te.segmentName) return seg.id;
    return -1;
}

time_t TrackManager::ComputeOccurrenceStart(const TimerEngine& engine, const TrackedEvent& te,
                                             time_t utcNow, int nowMin) const {
    auto* ev = FindEventDef(engine, te.wikiKey);
    if (!ev) return 0;
    int segId = FindSegmentId(engine, te);
    if (segId < 0) return 0;

    // Check if currently in this segment
    auto pi = engine.GetPhaseAt(*ev, nowMin);
    if (pi.segment && pi.segment->id == segId) {
        int segStartMin = nowMin - pi.elapsedInPhase;
        time_t dayStart = utcNow - (nowMin * 60 + (utcNow % 60));
        return dayStart + segStartMin * 60;
    }

    // Find next occurrence
    int minsUntil = engine.MinutesUntilSegment(*ev, segId, nowMin);
    if (minsUntil >= 9999) return 0;

    time_t dayStart = utcNow - (nowMin * 60 + (utcNow % 60));
    int nextStartMin = nowMin + minsUntil;
    return dayStart + nextStartMin * 60;
}

void TrackManager::AddTrack(const std::string& wikiKey, const std::string& segmentName,
                             const TimerEngine& engine, time_t utcNow) {
    if (IsTracked(wikiKey, segmentName)) return;

    struct tm utcTm;
#ifdef _WIN32
    gmtime_s(&utcTm, &utcNow);
#else
    gmtime_r(&utcNow, &utcTm);
#endif
    int nowMin = utcTm.tm_hour * 60 + utcTm.tm_min;

    TrackedEvent te;
    te.wikiKey = wikiKey;
    te.segmentName = segmentName;

    auto* ev = FindEventDef(engine, wikiKey);
    if (ev) {
        int segId = FindSegmentId(engine, te);
        if (segId >= 0) {
            auto pi = engine.GetPhaseAt(*ev, nowMin);
            bool isInSeg = (pi.segment && pi.segment->id == segId);
            int minsUntil = isInSeg ? 0 : engine.MinutesUntilSegment(*ev, segId, nowMin);

            time_t occStart = ComputeOccurrenceStart(engine, te, utcNow, nowMin);
            if (occStart > 0) {
                for (int i = 0; i < 3; ++i) {
                    if (minsUntil <= THRESHOLDS[i])
                        te.reminders[i].lastFiredOccStart = occStart;
                }
            }
        }
    }

    m_tracked.push_back(std::move(te));
}

void TrackManager::RemoveTrack(const std::string& wikiKey, const std::string& segmentName) {
    m_tracked.erase(
        std::remove_if(m_tracked.begin(), m_tracked.end(),
            [&](const TrackedEvent& te) {
                return te.wikiKey == wikiKey && te.segmentName == segmentName;
            }),
        m_tracked.end());
}

bool TrackManager::IsTracked(const std::string& wikiKey, const std::string& segmentName) const {
    for (auto& te : m_tracked)
        if (te.wikiKey == wikiKey && te.segmentName == segmentName) return true;
    return false;
}

std::vector<Notification> TrackManager::Tick(const TimerEngine& engine, time_t utcNow) {
    struct tm utcTm;
#ifdef _WIN32
    gmtime_s(&utcTm, &utcNow);
#else
    gmtime_r(&utcNow, &utcTm);
#endif
    int nowMin = utcTm.tm_hour * 60 + utcTm.tm_min;

    std::vector<Notification> result;

    // First tick after LoadPairs: suppress all current thresholds
    if (m_needsSuppression) {
        m_needsSuppression = false;
        for (auto& te : m_tracked) {
            auto* ev = FindEventDef(engine, te.wikiKey);
            if (!ev) continue;
            int segId = FindSegmentId(engine, te);
            if (segId < 0) continue;
            auto pi = engine.GetPhaseAt(*ev, nowMin);
            bool isInSeg = (pi.segment && pi.segment->id == segId);
            int mu = isInSeg ? 0 : engine.MinutesUntilSegment(*ev, segId, nowMin);
            time_t occ = ComputeOccurrenceStart(engine, te, utcNow, nowMin);
            if (occ > 0) {
                for (int i = 0; i < 3; ++i) {
                    if (mu <= THRESHOLDS[i])
                        te.reminders[i].lastFiredOccStart = occ;
                }
            }
        }
    }

    for (auto& te : m_tracked) {
        auto* ev = FindEventDef(engine, te.wikiKey);
        if (!ev) continue;
        int segId = FindSegmentId(engine, te);
        if (segId < 0) continue;

        auto pi = engine.GetPhaseAt(*ev, nowMin);
        bool isInSeg = (pi.segment && pi.segment->id == segId);
        int minsUntil = isInSeg ? 0 : engine.MinutesUntilSegment(*ev, segId, nowMin);

        time_t occStart = ComputeOccurrenceStart(engine, te, utcNow, nowMin);
        if (occStart == 0) continue;

        // Find chatlink for this segment
        std::string chatlink;
        for (auto& seg : ev->segments)
            if (seg.id == segId) { chatlink = seg.chatlink; break; }

        // Fire only the tightest satisfied threshold, mark all looser ones as fired
        int firedIdx = -1;
        for (int i = 0; i < 3; ++i) {
            if (minsUntil <= THRESHOLDS[i] && te.reminders[i].lastFiredOccStart != occStart) {
                firedIdx = i;
            }
        }
        if (firedIdx >= 0) {
            // Mark all thresholds >= firedIdx as fired
            for (int j = 0; j <= firedIdx; ++j)
                te.reminders[j].lastFiredOccStart = occStart;

            Notification n;
            n.segmentName = te.segmentName;
            n.chatlink = chatlink;
            n.threshold = THRESHOLDS[firedIdx];
            n.minutesUntil = minsUntil;
            n.started = (minsUntil == 0 && isInSeg);
            result.push_back(std::move(n));
        }
    }

    return result;
}

std::vector<TrackRow> TrackManager::GetTrackList(const TimerEngine& engine, int nowMin) const {
    std::vector<TrackRow> rows;
    for (auto& te : m_tracked) {
        auto* ev = FindEventDef(engine, te.wikiKey);
        if (!ev) continue;
        int segId = FindSegmentId(engine, te);
        if (segId < 0) continue;

        TrackRow row;
        row.track = &te;
        row.displayName = te.segmentName;

        auto pi = engine.GetPhaseAt(*ev, nowMin);
        bool isInSeg = (pi.segment && pi.segment->id == segId);
        row.isActive = isInSeg;
        row.minutesUntilStart = isInSeg ? 0 : engine.MinutesUntilSegment(*ev, segId, nowMin);

        for (auto& seg : ev->segments)
            if (seg.id == segId) { row.chatlink = seg.chatlink; break; }

        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(), [](const TrackRow& a, const TrackRow& b) {
        if (a.isActive != b.isActive) return a.isActive;
        return a.minutesUntilStart < b.minutesUntilStart;
    });

    return rows;
}

std::vector<std::pair<std::string,std::string>> TrackManager::GetPairs() const {
    std::vector<std::pair<std::string,std::string>> result;
    for (auto& te : m_tracked)
        result.push_back({te.wikiKey, te.segmentName});
    return result;
}

void TrackManager::LoadPairs(const std::vector<std::pair<std::string,std::string>>& pairs) {
    m_tracked.clear();
    m_needsSuppression = true;
    for (auto& [k, s] : pairs) {
        TrackedEvent te;
        te.wikiKey = k;
        te.segmentName = s;
        m_tracked.push_back(std::move(te));
    }
}
