#include <Windows.h>
#include <string>
#include <filesystem>
#include <ctime>
#include <fstream>
#include <algorithm>
#include "nexus/Nexus.h"
#include "mumble/Mumble.h"
#include "imgui/imgui.h"
#include "core/TimerEngine.h"
#include "core/ConfigManager.h"
#include "icon_data.h"
#include "events_default.h"

void AddonLoad(AddonAPI_t* aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();

static constexpr int VER_MAJOR = 0;
static constexpr int VER_MINOR = 2;
static constexpr int VER_BUILD = 0;
#define CLE_VERSION_STR "0.2.0"

AddonDefinition_t AddonDef = {};
HMODULE hSelf = nullptr;
AddonAPI_t* APIDefs = nullptr;
NexusLinkData_t* NexusLink = nullptr;
Mumble::Data* MumbleLink = nullptr;

TimerEngine* g_timer = nullptr;
ConfigManager* g_config = nullptr;
std::string g_configPath;
std::string g_addonDir;
std::string g_dataPath;
bool g_showWindow = true;
ImFont* g_font = nullptr;
time_t g_lastUpdate = 0;
int g_nowMin = 0;

static const char* QA_ID = "QA_CLE_TIMER";
static const char* KB_ID = "KB_CLE_TOGGLE";

// Timeline constants
static constexpr float LABEL_WIDTH = 130.0f;
static constexpr float ROW_HEIGHT = 20.0f;
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

static void RenderTimelineBar(ImDrawList* dl, const EventDef& ev, const TimerEngine& engine,
                               float x0, float y0, float barW, float barH,
                               int windowStart, int windowEnd, int nowMin) {
    float ppm = barW / (float)(windowEnd - windowStart);

    // Background
    dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + barW, y0 + barH), IM_COL32(25, 30, 40, 200));

    // Draw segments
    int prevSegId = -999;
    float segStartPx = x0;
    const Segment* prevSeg = nullptr;

    for (int m = windowStart; m <= windowEnd + 1; ++m) {
        int absM = ((m % 1440) + 1440) % 1440;
        auto pi = engine.GetPhaseAt(ev, absM);
        int curSegId = pi.segment ? pi.segment->id : -1;

        bool flush = (curSegId != prevSegId) || (m == windowEnd + 1);
        if (flush && prevSeg && m > windowStart) {
            float segEndPx = x0 + (m - windowStart) * ppm;

            ImU32 col = prevSeg->isGap ? GapColor() : SegColorToImU32(prevSeg->color);
            dl->AddRectFilled(ImVec2(segStartPx, y0 + 1), ImVec2(segEndPx, y0 + barH - 1), col);

            // Text label if enough space
            if (!prevSeg->isGap && !prevSeg->name.empty()) {
                float segW = segEndPx - segStartPx;
                ImVec2 textSize = ImGui::CalcTextSize(prevSeg->name.c_str());
                if (textSize.x < segW - 4) {
                    float tx = segStartPx + (segW - textSize.x) * 0.5f;
                    float ty = y0 + (barH - textSize.y) * 0.5f;
                    dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 230), prevSeg->name.c_str());
                } else if (segW > 20) {
                    // Abbreviated - first 3 chars
                    std::string abbr = prevSeg->name.substr(0, std::min((size_t)4, prevSeg->name.size()));
                    ImVec2 abbrSize = ImGui::CalcTextSize(abbr.c_str());
                    if (abbrSize.x < segW - 2) {
                        float tx = segStartPx + 2;
                        float ty = y0 + (barH - abbrSize.y) * 0.5f;
                        dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 180), abbr.c_str());
                    }
                }
            }

            segStartPx = x0 + (m - windowStart) * ppm;
        }

        if (curSegId != prevSegId) {
            prevSegId = curSegId;
            prevSeg = pi.segment;
            segStartPx = x0 + (m - windowStart) * ppm;
        }
    }

    // Border
    dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + barW, y0 + barH), IM_COL32(60, 70, 85, 150), 0, 0, 1.0f);
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

    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);
    APIDefs->InputBinds_RegisterWithString(KB_ID, OnKeybind, "ALT+E");
    APIDefs->Textures_GetOrCreateFromMemory("ICON_CLE", (void*)ICON_CLE_PNG, ICON_CLE_PNG_SIZE);
    APIDefs->QuickAccess_Add(QA_ID, "ICON_CLE", "ICON_CLE", KB_ID, "Claymore Law Event Timer");

    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\segoeui.ttf");
    APIDefs->Fonts_AddFromFile("FONT_CLE", 13.0f, fontPath, OnFontReceived, nullptr);

    APIDefs->Log(LOGL_INFO, "CLE", "Claymore Law Event Timer v" CLE_VERSION_STR " loaded.");
}

void AddonUnload() {
    APIDefs->Fonts_Release("FONT_CLE", OnFontReceived);
    g_font = nullptr;
    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->InputBinds_Deregister(KB_ID);
    if (g_config) { g_config->Save(g_configPath); delete g_config; g_config = nullptr; }
    if (g_timer) { delete g_timer; g_timer = nullptr; }
}

void AddonRender() {
    if (!g_showWindow || !g_timer || !g_config) return;

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
    }

    ImFont* f = g_font;
    if (f) ImGui::PushFont(f);
    PushGW2Style(g_config->GetWindowAlpha());

    ImGui::SetNextWindowSizeConstraints(ImVec2(450, 200), ImVec2(1200, 900));
    if (ImGui::Begin("Claymore Law Event Timer##CLE", &g_showWindow,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar)) {

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetCursorScreenPos();
        float winW = ImGui::GetContentRegionAvail().x;

        // UTC + local time header
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

        // Begin scrollable area for timeline
        ImGui::BeginChild("##timeline", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
        dl = ImGui::GetWindowDrawList();

        int windowStart = g_nowMin - HALF_WINDOW;
        int windowEnd = g_nowMin + HALF_WINDOW;
        float barW = winW - LABEL_WIDTH - 8;
        if (barW < 100) barW = 100;

        // Time header
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        float timeHeaderX = cursor.x + LABEL_WIDTH;
        RenderTimeHeader(dl, timeHeaderX, cursor.y, barW, windowStart, windowEnd);
        ImGui::Dummy(ImVec2(LABEL_WIDTH + barW, TIME_HEADER_H));

        float nowLinePx = timeHeaderX + barW * 0.5f;
        float nowLineTop = cursor.y;

        // Event rows grouped by expansion
        const auto& allEvents = g_timer->GetAllEvents();

        // Group events by expansion
        for (int ei = 0; ei < static_cast<int>(Expansion::COUNT); ++ei) {
            Expansion exp = static_cast<Expansion>(ei);

            // Collect visible events for this expansion
            std::vector<const EventDef*> visEvents;
            for (auto& ev : allEvents) {
                if (ev.expansion != exp) continue;
                std::string key = ev.segmentFilter
                    ? (ev.wikiKey + "#" + ev.segmentFilter) : ev.wikiKey;
                if (!g_config->IsEventVisible(key)) continue;
                visEvents.push_back(&ev);
            }
            if (visEvents.empty()) continue;

            // Expansion header
            bool open = ImGui::CollapsingHeader(ExpansionName(exp), ImGuiTreeNodeFlags_DefaultOpen);
            if (!open) continue;

            for (auto* ev : visEvents) {
                cursor = ImGui::GetCursorScreenPos();
                float labelX = cursor.x;
                float barX = cursor.x + LABEL_WIDTH;
                float rowY = cursor.y;

                // Label
                std::string label = ev->name;
                if (ev->segmentFilter) label = ev->segmentFilter;
                ImVec2 labelSize = ImGui::CalcTextSize(label.c_str());

                // Truncate label to fit
                if (labelSize.x > LABEL_WIDTH - 8) {
                    while (label.size() > 3 && ImGui::CalcTextSize(label.c_str()).x > LABEL_WIDTH - 12)
                        label.pop_back();
                    label += "..";
                }

                dl->AddText(ImVec2(labelX + 4, rowY + (ROW_HEIGHT - labelSize.y) * 0.5f),
                           IM_COL32(200, 210, 220, 230), label.c_str());

                // Timeline bar
                RenderTimelineBar(dl, *ev, *g_timer, barX, rowY, barW, ROW_HEIGHT,
                                  windowStart, windowEnd, g_nowMin);

                // Tooltip on hover
                ImGui::SetCursorScreenPos(ImVec2(barX, rowY));
                ImGui::InvisibleButton(("##bar_" + ev->wikiKey +
                    (ev->segmentFilter ? std::string("#") + ev->segmentFilter : "")).c_str(),
                    ImVec2(barW, ROW_HEIGHT));

                if (ImGui::IsItemHovered()) {
                    auto pi = g_timer->GetPhaseAt(*ev, g_nowMin);
                    if (pi.segment) {
                        ImGui::BeginTooltip();
                        if (!pi.segment->isGap) {
                            ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.3f, 1.0f), "%s",
                                pi.segment->name.c_str());
                            ImGui::Text("%dd kaldi", pi.minutesUntilEnd);
                        } else {
                            ImGui::TextColored(COL_DIM, "Bekleniyor");
                        }
                        if (!pi.segment->chatlink.empty()) {
                            ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "%s",
                                pi.segment->chatlink.c_str());
                            ImGui::TextColored(COL_DIM, "Tikla: kopyala");
                        }
                        ImGui::EndTooltip();
                    }
                    if (ImGui::IsItemClicked() && pi.segment && !pi.segment->chatlink.empty())
                        ImGui::SetClipboardText(pi.segment->chatlink.c_str());
                }

                ImGui::SetCursorScreenPos(ImVec2(cursor.x, rowY + ROW_HEIGHT + 1));
            }
        }

        float nowLineBottom = ImGui::GetCursorScreenPos().y;

        // Now line (drawn last, on top of everything)
        RenderNowLine(dl, nowLinePx, nowLineTop, nowLineBottom);

        ImGui::EndChild();
    }
    ImGui::End();

    PopGW2Style();
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
