#include <Windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#include <string>
#include <filesystem>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <deque>
#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"
#include "core/TimerEngine.h"
#include "core/ConfigManager.h"
#include "core/TrackManager.h"
#include "icon_data.h"
#include "events_default.h"

void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();

static constexpr int VER_MAJOR = 0;
static constexpr int VER_MINOR = 4;
static constexpr int VER_BUILD = 3;
#define CLE_VERSION_STR "0.4.3"

AddonDefinition_t AddonDef = {};
HMODULE hSelf = nullptr;
AddonAPI_t* APIDefs = nullptr;
NexusLinkData_t* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

TimerEngine* g_timer = nullptr;
ConfigManager* g_config = nullptr;
TrackManager* g_trackMgr = nullptr;
std::string g_configPath;
std::string g_addonDir;
std::string g_dataPath;
bool g_showWindow = false;
bool g_showTrackPanel = true;
ImFont* g_font = nullptr;
time_t g_lastUpdate = 0;
int g_nowMin = 0;
uint32_t g_currentMapId = 0;
uint32_t g_lastScrollMapId = 0;

// Context menu state
static std::string s_ctxWikiKey;
static std::string s_ctxSegName;
static std::string s_ctxChatlink;
static bool s_ctxOpen = false;

// Cached track rows (updated once/sec, rendered every frame)
static std::vector<TrackRow> g_cachedTrackRows;

// Toast system
struct Toast {
    std::string text;
    std::string chatlink;
    float timer;
    float maxTimer;
};
static std::deque<Toast> g_toasts;

static const char* QA_ID = "QA_CLE_TIMER";
static const char* KB_ID = "KB_CLE_TOGGLE";

// Timeline constants
static constexpr float LABEL_WIDTH = 130.0f;
static constexpr float ROW_HEIGHT = 22.0f;
static constexpr float TIME_HEADER_H = 18.0f;
static constexpr float GROUP_HEADER_H = 22.0f;
static constexpr int HALF_WINDOW = 60;

// Colors
static const ImVec4 COL_GOLD(0.93f, 0.91f, 0.67f, 1.0f);
static const ImVec4 COL_DIM(0.45f, 0.45f, 0.45f, 1.0f);

static ImU32 SegColorToImU32(const SegColor& c, float alpha = 0.85f) {
    return IM_COL32(c.r, c.g, c.b, (int)(alpha * 255));
}

static ImU32 GapColor() {
    return IM_COL32(40, 45, 55, 180);
}

static ImU32 NowLineColor() {
    return IM_COL32(238, 232, 170, 220);
}

static void PushGW2Style(float alpha) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.07f, 0.09f, alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.93f, 0.91f, 0.67f, 0.25f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.08f, 0.09f, 0.12f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.10f, 0.12f, 0.16f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.14f, 0.16f, 0.22f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.20f, 0.22f, 0.28f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.24f, 0.26f, 0.32f, 0.90f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.06f, 0.07f, 0.09f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.93f, 0.91f, 0.67f, 0.25f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 1));
}

static void PopGW2Style() {
    ImGui::PopStyleVar(5);
    ImGui::PopStyleColor(9);
}

static void RenderTimeHeader(ImDrawList* dl, float x0, float y0, float barW,
                              int windowStart, int windowEnd) {
    float ppm = barW / (float)(windowEnd - windowStart);

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + barW, y0 + TIME_HEADER_H),
                      IM_COL32(20, 25, 35, 200));

    for (int m = windowStart; m <= windowEnd; ++m) {
        int absM = ((m % 1440) + 1440) % 1440;
        float px = x0 + (m - windowStart) * ppm;

        if (absM % 60 == 0) {
            int h = absM / 60;
            char buf[8];
            snprintf(buf, sizeof(buf), "%02d:00", h);
            dl->AddLine(ImVec2(px, y0), ImVec2(px, y0 + TIME_HEADER_H), IM_COL32(100, 110, 130, 150), 1.0f);
            dl->AddText(ImVec2(px + 3, y0 + 2), IM_COL32(160, 170, 190, 220), buf);
        } else if (absM % 30 == 0) {
            dl->AddLine(ImVec2(px, y0 + TIME_HEADER_H * 0.5f),
                       ImVec2(px, y0 + TIME_HEADER_H), IM_COL32(70, 80, 95, 120), 1.0f);
        }
    }
}

static bool IsLightColor(const SegColor& c) {
    float lum = 0.299f * c.r + 0.587f * c.g + 0.114f * c.b;
    return lum > 160;
}

static void DrawBarText(ImDrawList* dl, ImVec2 pos, float maxW, const char* text, ImU32 textCol) {
    dl->AddText(pos, textCol, text);
}

static void RenderTimelineBar(ImDrawList* dl, const EventDef& ev, const TimerEngine& engine,
                               float x0, float y0, float barW, float barH,
                               int windowStart, int windowEnd, int nowMin) {
    float ppm = barW / (float)(windowEnd - windowStart);
    const char* segFilter = ev.segmentFilter;

    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + barW, y0 + barH), IM_COL32(25, 30, 40, 200));

    struct SegBlock {
        float startPx, endPx;
        int startMin, endMin;
        const Segment* seg;
        bool isFiltered;
    };
    std::vector<SegBlock> blocks;

    int prevSegId = -999;
    float segStartPx = x0;
    int segStartMin = windowStart;
    const Segment* prevSeg = nullptr;

    for (int m = windowStart; m <= windowEnd + 1; ++m) {
        int absM = ((m % 1440) + 1440) % 1440;
        auto pi = engine.GetPhaseAt(ev, absM);
        int curSegId = pi.segment ? pi.segment->id : -1;

        if ((curSegId != prevSegId) || (m == windowEnd + 1)) {
            if (prevSeg && m > windowStart) {
                float segEndPx = x0 + (m - windowStart) * ppm;
                bool filtered = segFilter && !prevSeg->isGap && prevSeg->name != segFilter;
                blocks.push_back({segStartPx, segEndPx, segStartMin, m, prevSeg, filtered});
            }
            prevSegId = curSegId;
            prevSeg = pi.segment;
            segStartPx = x0 + (std::max(m, windowStart) - windowStart) * ppm;
            segStartMin = m;
        }
    }

    float nowPx = x0 + (nowMin - windowStart) * ppm;

    for (auto& blk : blocks) {
        bool isGapOrFiltered = blk.seg->isGap || blk.isFiltered;
        bool isHighlight = !isGapOrFiltered && TimerEngine::IsHighlightSegment(ev, *blk.seg);

        ImU32 col;
        if (isGapOrFiltered) {
            col = GapColor();
        } else if (isHighlight) {
            col = SegColorToImU32(blk.seg->color, 0.90f);
        } else {
            // Non-highlight active segment: muted (blend toward gap)
            col = IM_COL32(
                (blk.seg->color.r + 50) / 3,
                (blk.seg->color.g + 55) / 3,
                (blk.seg->color.b + 65) / 3,
                160);
        }
        dl->AddRectFilled(ImVec2(blk.startPx, y0 + 1), ImVec2(blk.endPx, y0 + barH - 1), col);

        float segW = blk.endPx - blk.startPx;
        float ty = y0 + (barH - ImGui::GetTextLineHeight()) * 0.5f;

        if (!isGapOrFiltered && !blk.seg->name.empty()) {
            ImU32 textCol;
            if (isHighlight) {
                textCol = IsLightColor(blk.seg->color)
                    ? IM_COL32(10, 10, 10, 255) : IM_COL32(255, 255, 255, 245);
            } else {
                textCol = IM_COL32(180, 185, 195, 180);
            }

            ImVec2 nameSize = ImGui::CalcTextSize(blk.seg->name.c_str());
            if (nameSize.x < segW - 6) {
                float tx = blk.startPx + (segW - nameSize.x) * 0.5f;
                DrawBarText(dl, ImVec2(tx, ty), segW, blk.seg->name.c_str(), textCol);
            }
        }
    }

    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + barW, y0 + barH), IM_COL32(60, 70, 85, 120), 0, 0, 1.0f);
}

static void RenderNowLine(ImDrawList* dl, float x, float yTop, float yBottom) {
    dl->AddLine(ImVec2(x, yTop), ImVec2(x, yBottom), NowLineColor(), 2.0f);
    // Small triangle at top
    dl->AddTriangleFilled(
        ImVec2(x - 4, yTop), ImVec2(x + 4, yTop), ImVec2(x, yTop + 5), NowLineColor());
}

void OnFontReceived(const char* aIdentifier, void* aFont) {
    g_font = (ImFont*)aFont;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason, LPVOID) {
    if (ul_reason == DLL_PROCESS_ATTACH) hSelf = hModule;
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition_t* GetAddonDef() {
    AddonDef.Signature = -77044;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "Claymore Law Event Timer";
    AddonDef.Version.Major = VER_MAJOR;
    AddonDef.Version.Minor = VER_MINOR;
    AddonDef.Version.Build = VER_BUILD;
    AddonDef.Version.Revision = 0;
    AddonDef.Author = "Onur";
    AddonDef.Description = "GW2 Event Timer - Wiki tarzi timeline gorunumu";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = AF_None;
    AddonDef.Provider = UP_GitHub;
    AddonDef.UpdateLink = "https://github.com/FScaley/GW2-CLE-Timer";
    return &AddonDef;
}

void OnKeybind(const char* aIdentifier, bool aIsRelease) {
    if (!aIsRelease && std::string(aIdentifier) == KB_ID)
        g_showWindow = !g_showWindow;
}

void AddonLoad(AddonAPI_t* aApi) {
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext*)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions(
        (void*(*)(size_t, void*))APIDefs->ImguiMalloc,
        (void(*)(void*, void*))APIDefs->ImguiFree);

    NexusLink = (NexusLinkData_t*)APIDefs->DataLink_Get("DL_NEXUS_LINK");
    MumbleLink = (Mumble::Data*)APIDefs->DataLink_Get("DL_MUMBLE_LINK");

    g_addonDir = APIDefs->Paths_GetAddonDirectory("claymore-event-timer");
    std::filesystem::create_directories(g_addonDir);
    g_configPath = g_addonDir + "\\config.json";
    g_dataPath = g_addonDir + "\\events.json";

    if (!std::filesystem::exists(g_dataPath)) {
        std::ofstream out(g_dataPath, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(EVENTS_DEFAULT_JSON), EVENTS_DEFAULT_JSON_SIZE);
            out.close();
        }
    }

    g_config = new ConfigManager();
    g_config->Load(g_configPath);

    g_timer = new TimerEngine();
    bool loaded = g_timer->LoadFromFile(g_dataPath);
    if (!loaded)
        loaded = g_timer->LoadFromMemory(EVENTS_DEFAULT_JSON, EVENTS_DEFAULT_JSON_SIZE);

    if (loaded) {
        for (auto& ev : g_timer->GetAllEvents())
            if (!ev.defaultVisible)
                g_config->SetDefaultVisibility(ev.wikiKey, false);
    }

    g_trackMgr = new TrackManager();
    g_trackMgr->LoadPairs(g_config->GetTracked());

    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);
    APIDefs->InputBinds_RegisterWithString(KB_ID, OnKeybind, "ALT+E");
    APIDefs->Textures_GetOrCreateFromMemory("ICON_CLE", (void*)ICON_CLE_PNG, ICON_CLE_PNG_SIZE);
    APIDefs->QuickAccess_Add(QA_ID, "ICON_CLE", "ICON_CLE", KB_ID, "Claymore Law Event Timer");

    APIDefs->GUI_RegisterCloseOnEscape("Claymore Law Event Timer v" CLE_VERSION_STR "##CLE", &g_showWindow);

    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\segoeui.ttf");
    APIDefs->Fonts_AddFromFile("FONT_CLE", 14.0f, fontPath, OnFontReceived, nullptr);

    APIDefs->Log(LOGL_INFO, "CLE", "Claymore Law Event Timer v" CLE_VERSION_STR " loaded.");
}

void AddonUnload() {
    APIDefs->Fonts_Release("FONT_CLE", OnFontReceived);
    g_font = nullptr;
    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->GUI_DeregisterCloseOnEscape("Claymore Law Event Timer v" CLE_VERSION_STR "##CLE");
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->InputBinds_Deregister(KB_ID);
    if (g_trackMgr && g_config) {
        g_config->SetTracked(g_trackMgr->GetPairs());
    }
    if (g_config) { g_config->Save(g_configPath); delete g_config; g_config = nullptr; }
    if (g_trackMgr) { delete g_trackMgr; g_trackMgr = nullptr; }
    if (g_timer) { delete g_timer; g_timer = nullptr; }
}

void AddonRender() {
    if (!g_timer || !g_config || !g_trackMgr) return;

    ImFont* f = g_font;
    if (f) ImGui::PushFont(f);

    // ========== ONCE PER SECOND: update state ==========
    time_t now = time(nullptr);
    if (now != g_lastUpdate) {
        g_lastUpdate = now;
        struct tm utcTm;
#ifdef _WIN32
        gmtime_s(&utcTm, &now);
#else
        gmtime_r(&now, &utcTm);
#endif
        g_nowMin = utcTm.tm_hour * 60 + utcTm.tm_min;
        g_timer->Update(now);
        if (MumbleLink) g_currentMapId = MumbleLink->Context.MapID;

        // Track tick + notifications
        auto notifications = g_trackMgr->Tick(*g_timer, now);
        for (auto& n : notifications) {
            char alertBuf[256];
            if (n.started)
                snprintf(alertBuf, sizeof(alertBuf), "%s BASLADI!", n.segmentName.c_str());
            else
                snprintf(alertBuf, sizeof(alertBuf), "%s - %d dk sonra", n.segmentName.c_str(), n.minutesUntil);
            APIDefs->GUI_SendAlert(alertBuf);
            g_toasts.push_back({alertBuf, n.chatlink, 8.0f, 8.0f});
            if (g_config->GetSoundEnabled())
                PlaySound(TEXT("SystemAsterisk"), NULL, SND_ALIAS | SND_ASYNC);
        }

        g_cachedTrackRows = g_trackMgr->GetTrackList(*g_timer, g_nowMin);
    }

    // ========== MAIN TIMER WINDOW ==========
    if (g_showWindow) {
        PushGW2Style(g_config->GetWindowAlpha());
        ImGui::SetNextWindowSizeConstraints(ImVec2(450, 200), ImVec2(1200, 900));
        if (ImGui::Begin("Claymore Law Event Timer v" CLE_VERSION_STR "##CLE", &g_showWindow,
                ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

            ImDrawList* dl = ImGui::GetWindowDrawList();
            float winW = ImGui::GetContentRegionAvail().x;

            // Time header + Takip toggle
            char timeBuf[64];
            snprintf(timeBuf, sizeof(timeBuf), "UTC %02d:%02d", g_nowMin / 60, g_nowMin % 60);
            ImGui::TextColored(COL_GOLD, "%s", timeBuf);
            if (g_config->GetShowLocalTime()) {
                struct tm localTm;
#ifdef _WIN32
                localtime_s(&localTm, &now);
#else
                localtime_r(&now, &localTm);
#endif
                ImGui::SameLine();
                snprintf(timeBuf, sizeof(timeBuf), "| Yerel %02d:%02d", localTm.tm_hour, localTm.tm_min);
                ImGui::TextColored(COL_DIM, "%s", timeBuf);
            }
            ImGui::SameLine(winW - 50);
            if (ImGui::SmallButton(g_showTrackPanel ? "Takip<<" : "Takip>>"))
                g_showTrackPanel = !g_showTrackPanel;

            // Layout: timeline left, track panel right (docked)
            static constexpr float TRACK_PANEL_W = 200.0f;
            bool showTrack = g_showTrackPanel && !g_cachedTrackRows.empty();
            float timelineW = showTrack ? (winW - TRACK_PANEL_W - 6) : winW;
            if (timelineW < 300) timelineW = 300;

            ImGui::BeginChild("##timeline", ImVec2(timelineW, 0), false, 0);
            dl = ImGui::GetWindowDrawList();

            int windowStart = g_nowMin - HALF_WINDOW;
            int windowEnd = g_nowMin + HALF_WINDOW;
            float barW = timelineW - LABEL_WIDTH - 8;
            if (barW < 100) barW = 100;

            ImVec2 cursor = ImGui::GetCursorScreenPos();
            float timeHeaderX = cursor.x + LABEL_WIDTH;
            RenderTimeHeader(dl, timeHeaderX, cursor.y, barW, windowStart, windowEnd);
            ImGui::Dummy(ImVec2(LABEL_WIDTH + barW, TIME_HEADER_H));

            float nowLinePx = timeHeaderX + barW * 0.5f;
            float nowLineTop = cursor.y;

            const auto& allEvents = g_timer->GetAllEvents();

            for (int ei = 0; ei < static_cast<int>(Expansion::COUNT); ++ei) {
                Expansion exp = static_cast<Expansion>(ei);
                std::vector<const EventDef*> visEvents;
                for (auto& ev : allEvents) {
                    if (ev.expansion != exp) continue;
                    std::string key = ev.segmentFilter
                        ? (ev.wikiKey + "#" + ev.segmentFilter) : ev.wikiKey;
                    if (!g_config->IsEventVisible(key)) continue;
                    visEvents.push_back(&ev);
                }
                if (visEvents.empty()) continue;

                if (!ImGui::CollapsingHeader(ExpansionName(exp), ImGuiTreeNodeFlags_DefaultOpen))
                    continue;

                for (auto* ev : visEvents) {
                    cursor = ImGui::GetCursorScreenPos();
                    float labelX = cursor.x;
                    float barX = cursor.x + LABEL_WIDTH;
                    float rowY = cursor.y;

                    std::string label = ev->displayName;
                    if (ev->segmentFilter && ev->displayName == ev->name) label = ev->segmentFilter;
                    ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());
                    if (labelSize.x > LABEL_WIDTH - 8) {
                        while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > LABEL_WIDTH - 12)
                            label.pop_back();
                        label += "..";
                    }
                    dl->AddText(ImVec2(labelX + 4, rowY + (ROW_HEIGHT - labelSize.y) * 0.5f),
                               IM_COL32(200, 210, 220, 230), label.c_str());

                    bool isCurrentMap = ev->mapId != 0 && ev->mapId == g_currentMapId;
                    if (isCurrentMap) {
                        dl->AddRect(ImVec2(labelX, rowY - 1), ImVec2(barX + barW, rowY + ROW_HEIGHT + 1),
                                   IM_COL32(238, 232, 170, 180), 0, 0, 2.0f);
                        if (g_currentMapId != g_lastScrollMapId) {
                            ImGui::SetScrollHereY(0.3f);
                            g_lastScrollMapId = g_currentMapId;
                        }
                    }

                    RenderTimelineBar(dl, *ev, *g_timer, barX, rowY, barW, ROW_HEIGHT,
                                      windowStart, windowEnd, g_nowMin);

                    ImGui::SetCursorScreenPos(ImVec2(barX, rowY));
                    std::string btnId = "##bar_" + ev->wikiKey +
                        (ev->segmentFilter ? std::string("#") + ev->segmentFilter : "");
                    ImGui::InvisibleButton(btnId.c_str(), ImVec2(barW, ROW_HEIGHT));

                    float mouseX = ImGui::GetMousePos().x;
                    float ppm = barW / (float)(windowEnd - windowStart);
                    int mouseMin = windowStart + (int)((mouseX - barX) / ppm);
                    int absMouseMin = ((mouseMin % 1440) + 1440) % 1440;
                    auto pi = g_timer->GetPhaseAt(*ev, absMouseMin);

                    bool isFiltered = pi.segment && ev->segmentFilter && !pi.segment->isGap
                        && pi.segment->name != ev->segmentFilter;
                    int segStartMin = pi.segment ? (mouseMin - pi.elapsedInPhase) : 0;
                    int segEndMin = pi.segment ? (segStartMin + pi.phaseDuration) : 0;
                    bool isCurrent = segStartMin <= g_nowMin && segEndMin > g_nowMin;
                    bool isFuture = segStartMin > g_nowMin;

                    if (ImGui::IsItemHovered() && pi.segment) {
                        ImGui::BeginTooltip();
                        if (!pi.segment->isGap && !isFiltered) {
                            if (isCurrent) {
                                ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "%s", pi.segment->name.c_str());
                                ImGui::Text("%d dk icinde bitiyor", segEndMin - g_nowMin);
                            } else if (isFuture) {
                                ImGui::TextColored(ImVec4(0.9f, 0.85f, 0.2f, 1.0f), "%s", pi.segment->name.c_str());
                                ImGui::Text("%d dk sonra basliyor", segStartMin - g_nowMin);
                            } else {
                                ImGui::TextColored(COL_DIM, "%s (bitti)", pi.segment->name.c_str());
                            }
                        } else {
                            ImGui::TextColored(COL_DIM, "Bos alan");
                        }
                        if (pi.segment && !pi.segment->chatlink.empty() && !isFiltered)
                            ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "%s", pi.segment->chatlink.c_str());
                        ImGui::EndTooltip();
                    }

                    if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && pi.segment
                        && !pi.segment->chatlink.empty() && !isFiltered)
                        ImGui::SetClipboardText(pi.segment->chatlink.c_str());

                    // Right click: set context and open popup
                    if (ImGui::IsItemClicked(ImGuiMouseButton_Right) && pi.segment
                        && !pi.segment->isGap && !isFiltered) {
                        s_ctxWikiKey = ev->wikiKey;
                        s_ctxSegName = pi.segment->name;
                        s_ctxChatlink = pi.segment->chatlink;
                        s_ctxOpen = true;
                    }

                    ImGui::SetCursorScreenPos(ImVec2(cursor.x, rowY + ROW_HEIGHT + 1));
                }
            }

            // Context menu -- OUTSIDE the row loop, ONE popup
            if (s_ctxOpen) {
                ImGui::OpenPopup("##cle_ctx");
                s_ctxOpen = false;
            }
            if (ImGui::BeginPopup("##cle_ctx")) {
                bool tracked = g_trackMgr->IsTracked(s_ctxWikiKey, s_ctxSegName);
                if (!tracked) {
                    if (ImGui::MenuItem(("Takip et: " + s_ctxSegName).c_str())) {
                        g_trackMgr->AddTrack(s_ctxWikiKey, s_ctxSegName, *g_timer, now);
                        g_config->SetTracked(g_trackMgr->GetPairs());
                        g_config->Save(g_configPath);
                        g_showTrackPanel = true;
                    }
                } else {
                    if (ImGui::MenuItem(("Takipten cikar: " + s_ctxSegName).c_str())) {
                        g_trackMgr->RemoveTrack(s_ctxWikiKey, s_ctxSegName);
                        g_config->SetTracked(g_trackMgr->GetPairs());
                        g_config->Save(g_configPath);
                    }
                }
                if (!s_ctxChatlink.empty() && ImGui::MenuItem("Waypoint kopyala"))
                    ImGui::SetClipboardText(s_ctxChatlink.c_str());
                ImGui::EndPopup();
            }

            float nowLineBottom = ImGui::GetCursorScreenPos().y;
            RenderNowLine(dl, nowLinePx, nowLineTop, nowLineBottom);
            ImGui::EndChild();

            // ========== DOCKED TRACK PANEL (right side, inside main window) ==========
            if (showTrack) {
                ImGui::SameLine(0, 2);

                ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.04f, 0.05f, 0.07f, 0.70f));
                ImGui::BeginChild("##trackpanel", ImVec2(TRACK_PANEL_W, 0), true);

                ImGui::TextColored(COL_GOLD, "Takip");
                ImGui::Separator();

                for (size_t i = 0; i < g_cachedTrackRows.size(); ++i) {
                    auto& row = g_cachedTrackRows[i];
                    ImGui::PushID(static_cast<int>(i));

                    ImVec4 nameCol = row.isActive
                        ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                        : (row.minutesUntilStart <= 10
                            ? ImVec4(1.0f, 0.85f, 0.2f, 1.0f)
                            : ImVec4(0.85f, 0.87f, 0.90f, 1.0f));

                    // Remove button (left)
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.1f, 0.1f, 0.6f));
                    if (ImGui::SmallButton("X")) {
                        g_trackMgr->RemoveTrack(row.track->wikiKey, row.track->segmentName);
                        g_config->SetTracked(g_trackMgr->GetPairs());
                        g_config->Save(g_configPath);
                        g_cachedTrackRows = g_trackMgr->GetTrackList(*g_timer, g_nowMin);
                        ImGui::PopStyleColor();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    ImGui::TextColored(nameCol, "%s", row.displayName.c_str());

                    // Countdown (right-aligned)
                    char cdBuf[32];
                    if (row.isActive) snprintf(cdBuf, sizeof(cdBuf), "AKTIF");
                    else {
                        int h = row.minutesUntilStart / 60, m = row.minutesUntilStart % 60;
                        if (h > 0) snprintf(cdBuf, sizeof(cdBuf), "%ds%02dd", h, m);
                        else snprintf(cdBuf, sizeof(cdBuf), "%ddk", m);
                    }
                    float cdW = ImGui::CalcTextSize(cdBuf).x;
                    float avail = ImGui::GetContentRegionAvail().x;
                    ImGui::SameLine(avail - cdW + ImGui::GetCursorPosX());
                    ImGui::TextColored(nameCol, "%s", cdBuf);

                    // WP on hover
                    if (ImGui::IsItemHovered() && !row.chatlink.empty()) {
                        ImGui::BeginTooltip();
                        ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "%s", row.chatlink.c_str());
                        ImGui::TextColored(COL_DIM, "Tikla: kopyala");
                        ImGui::EndTooltip();
                        if (ImGui::IsItemClicked())
                            ImGui::SetClipboardText(row.chatlink.c_str());
                    }

                    ImGui::PopID();
                }

                if (g_cachedTrackRows.empty()) {
                    ImGui::TextColored(COL_DIM, "Sag tik -> Takip et");
                }

                ImGui::EndChild();
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
        PopGW2Style();
    }

    // ========== TOASTS (always visible) ==========
    if (!g_toasts.empty()) {
        ImGuiIO& io = ImGui::GetIO();
        float dt = io.DeltaTime;
        float toastW = 300;
        float yOffset = 60;

        for (size_t i = 0; i < g_toasts.size(); ++i) {
            auto& t = g_toasts[i];
            t.timer -= dt;
            if (t.timer <= 0) continue;

            float alpha = (t.timer < 1.5f) ? (t.timer / 1.5f) : 1.0f;
            ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - toastW - 20, yOffset));
            ImGui::SetNextWindowSize(ImVec2(toastW, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.92f * alpha));
            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.93f, 0.91f, 0.67f, 0.5f * alpha));

            char toastId[32];
            snprintf(toastId, sizeof(toastId), "##cle_toast_%zu", i);
            ImGui::Begin(toastId, nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize
                | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);
            ImGui::TextColored(ImVec4(0.93f, 0.91f, 0.67f, alpha), "%s", t.text.c_str());
            if (!t.chatlink.empty()) {
                ImGui::SameLine();
                if (ImGui::SmallButton("WP")) ImGui::SetClipboardText(t.chatlink.c_str());
            }
            if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                t.timer = 0;
            float toastH = ImGui::GetWindowSize().y;
            ImGui::End();
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            yOffset += toastH + 4;
        }

        // Remove all expired
        g_toasts.erase(std::remove_if(g_toasts.begin(), g_toasts.end(),
            [](const Toast& t) { return t.timer <= 0; }), g_toasts.end());
    }

    if (f) ImGui::PopFont();
}

void AddonOptions() {
    if (!g_config || !g_timer) return;
    ImFont* f = g_font;
    if (f) ImGui::PushFont(f);

    ImGui::Text("Claymore Law Event Timer v" CLE_VERSION_STR);
    ImGui::Separator();

    bool showLocal = g_config->GetShowLocalTime();
    if (ImGui::Checkbox("Yerel saat goster", &showLocal)) {
        g_config->SetShowLocalTime(showLocal);
        g_config->Save(g_configPath);
    }

    float alpha = g_config->GetWindowAlpha();
    if (ImGui::SliderFloat("Pencere seffafligi", &alpha, 0.3f, 1.0f, "%.2f")) {
        g_config->SetWindowAlpha(alpha);
        g_config->Save(g_configPath);
    }

    bool sound = g_config->GetSoundEnabled();
    if (ImGui::Checkbox("Bildirim sesi", &sound)) {
        g_config->SetSoundEnabled(sound);
        g_config->Save(g_configPath);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Gorunur Eventler:");

    const auto& allEvents = g_timer->GetAllEvents();
    for (int ei = 0; ei < static_cast<int>(Expansion::COUNT); ++ei) {
        Expansion exp = static_cast<Expansion>(ei);
        bool hasAny = false;
        for (auto& ev : allEvents)
            if (ev.expansion == exp) { hasAny = true; break; }
        if (!hasAny) continue;

        if (ImGui::TreeNode(ExpansionName(exp))) {
            for (auto& ev : allEvents) {
                if (ev.expansion != exp) continue;
                std::string key = ev.segmentFilter
                    ? (ev.wikiKey + "#" + ev.segmentFilter) : ev.wikiKey;
                std::string label = ev.segmentFilter ? ev.segmentFilter : ev.name;
                bool visible = g_config->IsEventVisible(key);
                if (ImGui::Checkbox((label + "##" + key).c_str(), &visible)) {
                    g_config->SetEventVisible(key, visible);
                    g_config->Save(g_configPath);
                }
            }
            ImGui::TreePop();
        }
    }

    if (f) ImGui::PopFont();
}
