#include <Windows.h>
#include <string>
#include <filesystem>
#include <ctime>
#include <fstream>
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
static constexpr int VER_MINOR = 1;
static constexpr int VER_BUILD = 0;
#define CLE_VERSION_STR "0.1.0"

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

static const char* QA_ID = "QA_CLE_TIMER";
static const char* KB_ID = "KB_CLE_TOGGLE";

static const ImVec4 COL_GOLD(0.93f, 0.91f, 0.67f, 1.0f);
static const ImVec4 COL_ACTIVE(0.30f, 0.85f, 0.30f, 1.0f);
static const ImVec4 COL_SOON(1.00f, 0.85f, 0.20f, 1.0f);
static const ImVec4 COL_UPCOMING(0.70f, 0.70f, 0.70f, 1.0f);
static const ImVec4 COL_DIM(0.45f, 0.45f, 0.45f, 1.0f);
static const ImVec4 COL_CATEGORY(0.80f, 0.70f, 0.40f, 1.0f);
static const ImVec4 COL_CHATLINK(0.55f, 0.75f, 1.0f, 1.0f);

static void PushGW2Style(float alpha) {
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.05f, 0.04f, 0.03f, alpha));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.93f, 0.91f, 0.67f, 0.30f));
    ImGui::PushStyleColor(ImGuiCol_TitleBg, ImVec4(0.10f, 0.08f, 0.05f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(0.15f, 0.12f, 0.07f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.17f, 0.10f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.30f, 0.25f, 0.15f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.35f, 0.30f, 0.18f, 0.80f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4(0.05f, 0.04f, 0.03f, 0.50f));
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ImVec4(0.93f, 0.91f, 0.67f, 0.30f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
}

static void PopGW2Style() {
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(9);
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
    AddonDef.Description = "GW2 Event Timer - Expansion bazli meta/boss zamanlayici";
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

    // Write embedded default if no external events.json exists
    if (!std::filesystem::exists(g_dataPath)) {
        std::ofstream out(g_dataPath, std::ios::binary);
        if (out.is_open()) {
            out.write(reinterpret_cast<const char*>(EVENTS_DEFAULT_JSON), EVENTS_DEFAULT_JSON_SIZE);
            out.close();
            APIDefs->Log(LOGL_INFO, "CLE", "Default events.json written to addon directory.");
        }
    }

    g_config = new ConfigManager();
    g_config->Load(g_configPath);

    g_timer = new TimerEngine();
    bool loaded = g_timer->LoadFromFile(g_dataPath);
    if (!loaded) {
        loaded = g_timer->LoadFromMemory(EVENTS_DEFAULT_JSON, EVENTS_DEFAULT_JSON_SIZE);
    }

    if (loaded) {
        for (auto& ev : g_timer->GetAllEvents()) {
            if (!ev.defaultVisible)
                g_config->SetDefaultVisibility(ev.wikiKey, false);
        }
        APIDefs->Log(LOGL_INFO, "CLE", "Event data loaded.");
    } else {
        APIDefs->Log(LOGL_WARNING, "CLE", "Event data could not be loaded!");
    }

    APIDefs->GUI_Register(RT_Render, AddonRender);
    APIDefs->GUI_Register(RT_OptionsRender, AddonOptions);
    APIDefs->InputBinds_RegisterWithString(KB_ID, OnKeybind, "ALT+E");
    APIDefs->Textures_GetOrCreateFromMemory("ICON_CLE", (void*)ICON_CLE_PNG, ICON_CLE_PNG_SIZE);
    APIDefs->QuickAccess_Add(QA_ID, "ICON_CLE", "ICON_CLE", KB_ID, "Claymore Law Event Timer");

    char fontPath[MAX_PATH];
    GetWindowsDirectoryA(fontPath, MAX_PATH);
    strcat_s(fontPath, "\\Fonts\\segoeui.ttf");
    APIDefs->Fonts_AddFromFile("FONT_CLE", 16.0f, fontPath, OnFontReceived, nullptr);

    APIDefs->Log(LOGL_INFO, "CLE", "Claymore Law Event Timer v" CLE_VERSION_STR " loaded.");
}

void AddonUnload() {
    APIDefs->Fonts_Release("FONT_CLE", OnFontReceived);
    g_font = nullptr;

    APIDefs->GUI_Deregister(AddonRender);
    APIDefs->GUI_Deregister(AddonOptions);
    APIDefs->QuickAccess_Remove(QA_ID);
    APIDefs->InputBinds_Deregister(KB_ID);

    if (g_config) {
        g_config->Save(g_configPath);
        delete g_config; g_config = nullptr;
    }
    if (g_timer) { delete g_timer; g_timer = nullptr; }

    APIDefs->Log(LOGL_INFO, "CLE", "Claymore Law Event Timer unloaded.");
}

static std::string RowConfigKey(const EventRow& row) {
    if (row.def->segmentFilter)
        return row.def->wikiKey + "#" + row.def->segmentFilter;
    return row.def->wikiKey;
}

static void RenderEventRow(const EventRow& row, const std::string& configKey) {
    ImVec4 statusColor;
    const char* statusIcon;
    if (row.isActive) {
        statusColor = COL_ACTIVE;
        statusIcon = "\xe2\x96\xb6";
    } else if (row.minutesUntilStart <= 5) {
        statusColor = COL_SOON;
        statusIcon = "\xe2\x97\x8b";
    } else if (row.minutesUntilStart <= 15) {
        statusColor = ImVec4(1.0f, 0.65f, 0.20f, 1.0f);
        statusIcon = "\xe2\x97\x8b";
    } else {
        statusColor = COL_UPCOMING;
        statusIcon = "\xc2\xb7";
    }

    // Status icon
    ImGui::TextColored(statusColor, "%s", statusIcon);
    ImGui::SameLine(0, 6);

    // Selectable row for click-to-copy chatlink
    float availWidth = ImGui::GetContentRegionAvail().x;
    ImVec2 pos = ImGui::GetCursorScreenPos();

    ImGui::PushID(configKey.c_str());
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.3f, 0.2f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.3f, 0.2f, 0.5f));

    bool clicked = ImGui::Selectable("##row", false, 0, ImVec2(availWidth, 0));

    if (clicked && !row.chatlink.empty())
        ImGui::SetClipboardText(row.chatlink.c_str());

    if (ImGui::IsItemHovered() && !row.chatlink.empty()) {
        ImGui::BeginTooltip();
        ImGui::TextColored(COL_CHATLINK, "%s", row.chatlink.c_str());
        ImGui::TextColored(COL_DIM, "Tikla: panoya kopyala");
        ImGui::EndTooltip();
    }

    ImGui::PopStyleColor(3);
    ImGui::PopID();

    // Draw text on top of selectable via DrawList
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 col = ImGui::GetColorU32(statusColor);
    dl->AddText(pos, col, row.displayName.c_str());
    float cw = ImGui::CalcTextSize(row.countdown.c_str()).x;
    dl->AddText(ImVec2(pos.x + availWidth - cw, pos.y), col, row.countdown.c_str());
}

void AddonRender() {
    if (!g_showWindow || !g_timer || !g_config) return;

    time_t now = time(nullptr);
    if (now != g_lastUpdate) {
        g_lastUpdate = now;
        g_timer->Update(now);
    }

    ImFont* f = g_font;
    if (f) ImGui::PushFont(f);
    PushGW2Style(g_config->GetWindowAlpha());

    ImGui::SetNextWindowSizeConstraints(ImVec2(320, 200), ImVec2(600, 900));
    if (ImGui::Begin("Claymore Law Event Timer##CLE", &g_showWindow,
            ImGuiWindowFlags_NoCollapse)) {

        struct tm utcTm;
#ifdef _WIN32
        gmtime_s(&utcTm, &now);
#else
        gmtime_r(&now, &utcTm);
#endif
        char timeBuf[64];
        snprintf(timeBuf, sizeof(timeBuf), "UTC %02d:%02d", utcTm.tm_hour, utcTm.tm_min);
        ImGui::TextColored(COL_GOLD, "%s", timeBuf);

        if (g_config->GetShowLocalTime()) {
            struct tm localTm;
#ifdef _WIN32
            localtime_s(&localTm, &now);
#else
            localtime_r(&now, &localTm);
#endif
            snprintf(timeBuf, sizeof(timeBuf), " | Yerel %02d:%02d", localTm.tm_hour, localTm.tm_min);
            ImGui::SameLine();
            ImGui::TextColored(COL_DIM, "%s", timeBuf);
        }
        ImGui::Separator();

        const auto& groups = g_timer->GetGroups();
        for (auto& group : groups) {
            bool hasVisible = false;
            for (auto& row : group.rows) {
                std::string key = RowConfigKey(row);
                if (g_config->IsEventVisible(key)) {
                    hasVisible = true;
                    break;
                }
            }
            if (!hasVisible) continue;

            if (ImGui::CollapsingHeader(group.name.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                EventCategory lastCategory = EventCategory::COUNT;
                for (auto& row : group.rows) {
                    std::string key = RowConfigKey(row);
                    if (!g_config->IsEventVisible(key)) continue;

                    if (row.def->category != lastCategory) {
                        if (lastCategory != EventCategory::COUNT)
                            ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.80f, 0.70f, 0.40f, 1.0f),
                            "  %s", CategoryName(row.def->category));
                        lastCategory = row.def->category;
                    }

                    ImGui::Indent(8);
                    RenderEventRow(row, key);
                    ImGui::Unindent(8);
                }
            }
        }
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
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Istemedigin eventleri kapat.");

    const auto& groups = g_timer->GetGroups();
    for (auto& group : groups) {
        if (group.rows.empty()) continue;

        if (ImGui::TreeNode(group.name.c_str())) {
            for (auto& row : group.rows) {
                std::string key = RowConfigKey(row);
                bool visible = g_config->IsEventVisible(key);
                std::string label = row.displayName + "##" + key;
                if (ImGui::Checkbox(label.c_str(), &visible)) {
                    g_config->SetEventVisible(key, visible);
                    g_config->Save(g_configPath);
                }
            }
            ImGui::TreePop();
        }
    }

    if (f) ImGui::PopFont();
}
