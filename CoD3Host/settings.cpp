#include "settings.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include <Windows.h>

namespace
{
    std::mutex g_mutex;
    Settings::Values g_values;
    std::filesystem::path g_file;

    const char* const ActionNames[Settings::ActionCount] = {
        "Fire", "Aim", "Jump", "Crouch", "Use", "Reload", "Weapon", "Melee", "Sprint", "Binoculars",
        "Grenade", "Smoke", "Objectives", "Pause", "Forward", "Backward", "Left", "Right",
    };
    const char* const ActionKeys[Settings::ActionCount] = {
        "fire", "aim", "jump", "crouch", "use", "reload", "weapon", "melee", "sprint", "binoculars",
        "grenade", "smoke", "objectives", "pause", "forward", "backward", "left", "right",
    };

    std::string Trim(const std::string& text)
    {
        size_t a = 0, b = text.size();
        while (a < b && isspace((unsigned char)text[a])) a++;
        while (b > a && isspace((unsigned char)text[b - 1])) b--;
        return text.substr(a, b - a);
    }
}

const char* Settings::ActionName(Action action)
{
    return action >= 0 && action < ActionCount ? ActionNames[action] : "?";
}

const char* Settings::KeyName(int key)
{
    static char name[64];
    switch (key)
    {
    case 0: return "-";
    case VK_LBUTTON: return "Mouse 1";
    case VK_RBUTTON: return "Mouse 2";
    case VK_MBUTTON: return "Mouse 3";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_SPACE: return "Space";
    case VK_RETURN: return "Enter";
    case VK_ESCAPE: return "Escape";
    case VK_TAB: return "Tab";
    case VK_SHIFT: return "Shift";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_CONTROL: return "Ctrl";
    case VK_LCONTROL: return "Left Ctrl";
    case VK_RCONTROL: return "Right Ctrl";
    case VK_MENU: return "Alt";
    case VK_UP: return "Up";
    case VK_DOWN: return "Down";
    case VK_LEFT: return "Left";
    case VK_RIGHT: return "Right";
    case VK_BACK: return "Backspace";
    case VK_CAPITAL: return "Caps Lock";
    default: break;
    }
    if ((key >= '0' && key <= '9') || (key >= 'A' && key <= 'Z')) { name[0] = char(key); name[1] = 0; return name; }
    if (key >= VK_F1 && key <= VK_F12) { snprintf(name, sizeof(name), "F%d", key - VK_F1 + 1); return name; }
    if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9) { snprintf(name, sizeof(name), "Num %d", key - VK_NUMPAD0); return name; }
    // Anything else by what the keyboard layout calls it.
    const UINT scan = MapVirtualKeyW(UINT(key), MAPVK_VK_TO_VSC);
    wchar_t wide[64] = {};
    if (scan != 0 && GetKeyNameTextW(LONG(scan << 16), wide, 63) > 0)
    {
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, name, sizeof(name), nullptr, nullptr);
        return name;
    }
    snprintf(name, sizeof(name), "Key %d", key);
    return name;
}

Settings::Values Settings::Defaults()
{
    Values v;
    v.keys[Fire] = VK_LBUTTON;
    v.keys[Aim] = VK_RBUTTON;
    v.keys[Jump] = VK_SPACE;
    v.keys[Crouch] = 'C';
    v.keys[Use] = 'E';
    v.keys[Reload] = 'R';
    v.keys[Weapon] = 'F';
    v.keys[Melee] = 'V';
    v.keys[Sprint] = VK_SHIFT;
    v.keys[Binoculars] = 'B';
    v.keys[Grenade] = 'G';
    v.keys[Smoke] = '4';
    v.keys[Objectives] = VK_TAB;
    v.keys[Pause] = VK_ESCAPE;
    v.keys[Forward] = 'W';
    v.keys[Backward] = 'S';
    v.keys[Left] = 'A';
    v.keys[Right] = 'D';
    return v;
}

Settings::Values Settings::Get()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_values;
}

void Settings::Set(const Values& values)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_values = values;
    }
    Save();
}

void Settings::Preview(const Values& values)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_values = values;
}
void Settings::Load(const std::filesystem::path& exeDirectory, bool hungarianAvailable)
{
    g_file = exeDirectory / "CoD3Recomp.ini";
    Values v = Defaults();
    // Existing Hungarian installations keep their language when upgrading
    // from an INI that predates the selector.
    v.textLanguage = hungarianAvailable ? 1 : 0;
    if (FILE* file = _wfopen(g_file.c_str(), L"rb"))
    {
        char line[512];
        while (fgets(line, sizeof(line), file))
        {
            const std::string text = Trim(line);
            if (text.empty() || text[0] == '#' || text[0] == ';' || text[0] == '[') continue;
            const size_t eq = text.find('=');
            if (eq == std::string::npos) continue;
            const std::string key = Trim(text.substr(0, eq)), value = Trim(text.substr(eq + 1));
            if (key == "renderer") v.renderer = atoi(value.c_str());
            else if (key == "resolution")
            {
                // "1920x1080", or "desktop".
                int w = 0, h = 0;
                if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2 && w > 0 && h > 0) { v.resolutionWidth = w; v.resolutionHeight = h; }
            }
            else if (key == "resolution_scale") v.resolutionScale = atoi(value.c_str());
            else if (key == "texture_filter") v.textureFilter = atoi(value.c_str());
            else if (key == "frame_cap") v.frameCap = std::clamp(atoi(value.c_str()), 0, 1000);
            else if (key == "anisotropy") v.anisotropy = atoi(value.c_str());
            else if (key == "vsync") v.vsync = atoi(value.c_str()) != 0;
            else if (key == "unlock_frame_rate") v.unlockFrameRate = atoi(value.c_str()) != 0;
            else if (key == "antialiasing") v.antialiasing = atoi(value.c_str());
            else if (key == "texture_quality") v.textureQuality = atoi(value.c_str());
            else if (key == "fov") v.fov = atoi(value.c_str());
            else if (key == "ads_fov") v.adsFov = atoi(value.c_str());
            else if (key == "model_detail") v.modelDetail = atoi(value.c_str());
            else if (key == "shadow_quality") v.shadowQuality = atoi(value.c_str());
            else if (key == "aspect_ratio") v.aspectRatio = atoi(value.c_str());
            else if (key == "aim_blur") v.aimBlur = atoi(value.c_str()) != 0;
            else if (key == "console_depth") v.consoleDepth = atoi(value.c_str()) != 0;
            else if (key == "sharp_shadows") v.sharpShadows = atoi(value.c_str()) != 0;
            else if (key == "full_res_effects") v.effectsResolution = atoi(value.c_str()) != 0 ? 2 : 0;   // the older on/off key
            else if (key == "effects_resolution") v.effectsResolution = atoi(value.c_str());
            else if (key == "foliage_coverage") v.foliageCoverage = atoi(value.c_str()) != 0;
            else if (key == "fast_shadow_maps") v.fastShadowMaps = atoi(value.c_str()) != 0;
            else if (key == "steady_decals") v.steadyDecals = atoi(value.c_str()) != 0;
            else if (key == "native_commands") v.nativeCommands = atoi(value.c_str()) != 0;
            else if (key == "watch_writes") v.watchWrites = atoi(value.c_str()) != 0;
            else if (key == "film_video_levels") v.filmVideoLevels = atoi(value.c_str()) != 0;
            else if (key == "opt_fast_constants") v.fastConstants = atoi(value.c_str()) != 0;
            else if (key == "opt_shader_load_cache") v.shaderLoadCache = atoi(value.c_str()) != 0;
            else if (key == "opt_strong_change_check") v.strongChangeCheck = atoi(value.c_str()) != 0;
            else if (key == "opt_range_looks") v.rangeLooks = atoi(value.c_str()) != 0;
            else if (key == "opt_opaque_shaders") v.opaqueShaders = atoi(value.c_str()) != 0;
            else if (key == "opt_staging_uploads") v.stagingUploads = atoi(value.c_str()) != 0;
            else if (key == "opt_texture_pool") v.texturePool = atoi(value.c_str()) != 0;
            else if (key == "hdr") v.hdr = atoi(value.c_str()) != 0;
            else if (key == "hdr_tone_mapper") v.hdrToneMapper = std::clamp(atoi(value.c_str()), 0, 7);
            else if (key == "hdr_peak_nits") v.hdrPeakNits = std::clamp(float(atof(value.c_str())), 0.0f, 10000.0f);
            else if (key == "hdr_game_nits") v.hdrGameNits = std::clamp(float(atof(value.c_str())), 48.0f, 1000.0f);
            else if (key == "hdr_ui_nits") v.hdrUiNits = std::clamp(float(atof(value.c_str())), 48.0f, 1000.0f);
            else if (key == "hdr_gamma_correction") v.hdrGammaCorrection = std::clamp(atoi(value.c_str()), 0, 2);
            else if (key == "hdr_exposure") v.hdrExposure = std::clamp(float(atof(value.c_str())), 0.0f, 4.0f);
            else if (key == "hdr_highlights") v.hdrHighlights = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_shadows") v.hdrShadows = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_contrast") v.hdrContrast = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_saturation") v.hdrSaturation = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_highlight_saturation") v.hdrHighlightSaturation = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_blowout") v.hdrBlowout = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_flare") v.hdrFlare = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_hue_correction") v.hdrHueCorrection = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_cone_response") v.hdrConeResponse = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_bloom") v.hdrBloom = std::clamp(float(atof(value.c_str())), 0.0f, 100.0f);
            else if (key == "hdr_tv_ramp")
            {
                const float ramp = float(atof(value.c_str()));
                v.hdrTvRamp = std::clamp(ramp, 0.0f, 100.0f);
            }
            else if (key == "hdr_hud_match") v.hdrHudMatch = atoi(value.c_str()) != 0;
            else if (key == "hdr_film_upgrade") v.hdrFilmUpgrade = atoi(value.c_str()) != 0;
            else if (key == "window_mode") v.windowMode = atoi(value.c_str());
            else if (key == "fps_overlay") v.fpsOverlay = atoi(value.c_str()) != 0;
            else if (key == "stats_overlay") v.statsOverlay = atoi(value.c_str()) != 0;
            else if (key == "aim_assist") v.aimAssist = atoi(value.c_str()) != 0;
            else if (key == "subtitles") v.subtitles = atoi(value.c_str()) != 0;
            else if (key == "text_language") v.textLanguage = value == "hu" ? 1 : 0;
            else if (key == "console") v.console = atoi(value.c_str()) != 0;
            else if (key == "controller") v.controller = atoi(value.c_str()) != 0;
            else if (key == "button_prompts") v.buttonPrompts = std::clamp(atoi(value.c_str()), 0, 2);
            // The mouse looks straight into the view angles now; the old
            // mouse_sensitivity was a stick's gain and is left behind.
            else if (key == "look_sensitivity") v.mouseSensitivity = float(atof(value.c_str()));
            else if (key == "ads_sensitivity") v.adsSensitivity = float(atof(value.c_str()));
            else if (key == "aim_toggle") v.aimToggle = atoi(value.c_str()) != 0;
            else if (key == "master_volume") v.masterVolume = std::clamp(atoi(value.c_str()), 0, 100);
            else if (key == "audio_channels") v.audioChannels = std::clamp(atoi(value.c_str()), 0, 2);
            else if (key == "texture_dump") v.textureDump = atoi(value.c_str()) != 0;
            else if (key == "texture_replace") v.textureReplace = atoi(value.c_str()) != 0;
            else if (key == "raw_mouse") v.rawMouse = atoi(value.c_str()) != 0;
            else if (key == "auto_update") v.autoUpdate = atoi(value.c_str()) != 0;
            else if (key.compare(0, 4, "key_") == 0)
            {
                for (int i = 0; i < ActionCount; i++)
                    if (key.compare(4, std::string::npos, ActionKeys[i]) == 0) v.keys[i] = atoi(value.c_str());
            }
        }
        fclose(file);
    }
    if (v.renderer < 0 || v.renderer > 2) v.renderer = 0;
    if (v.resolutionWidth < 320 || v.resolutionHeight < 240 || v.resolutionWidth > 16384 || v.resolutionHeight > 16384) { v.resolutionWidth = 0; v.resolutionHeight = 0; }
    if (v.resolutionScale < 25 || v.resolutionScale > 200) v.resolutionScale = 100;
    if (v.textureFilter < 0 || v.textureFilter > 3) v.textureFilter = 3;
    if (v.anisotropy < 2 || v.anisotropy > 16) v.anisotropy = 16;
    if (v.antialiasing < 0 || v.antialiasing > 5) v.antialiasing = 3;
    if (v.textureQuality < 0 || v.textureQuality > 2) v.textureQuality = 2;
    if (v.fov < 65 || v.fov > 100) v.fov = 65;
    if (v.adsFov < 100 || v.adsFov > 250) v.adsFov = 100;
    if (v.modelDetail < 0 || v.modelDetail > 1) v.modelDetail = 1;
    if (v.shadowQuality < 0 || v.shadowQuality > 4) v.shadowQuality = 0;
    if (v.aspectRatio < 0 || v.aspectRatio > 3) v.aspectRatio = 0;
    if (v.windowMode < 0 || v.windowMode > 1) v.windowMode = 0;
    if (!(v.mouseSensitivity >= 0.05f && v.mouseSensitivity <= 5.0f)) v.mouseSensitivity = 1.0f;
    if (!hungarianAvailable) v.textLanguage = 0;
    if (!(v.adsSensitivity >= 0.1f && v.adsSensitivity <= 5.0f)) v.adsSensitivity = 1.5f;
    if (v.masterVolume < 0 || v.masterVolume > 100) v.masterVolume = 100;
    if (v.audioChannels < 0 || v.audioChannels > 2) v.audioChannels = 0;
    if (v.effectsResolution < 0 || v.effectsResolution > 2) v.effectsResolution = 0;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_values = v;
}

void Settings::Save()
{
    Values v = Get();
    if (g_file.empty()) return;
    if (FILE* file = _wfopen(g_file.c_str(), L"wb"))
    {
        fprintf(file, "# Call of Duty 3 recompiled: the settings menu (F11) keeps its values here.\n");
        char resolution[32];
        if (v.resolutionWidth > 0 && v.resolutionHeight > 0) snprintf(resolution, sizeof(resolution), "%dx%d", v.resolutionWidth, v.resolutionHeight);
        else snprintf(resolution, sizeof(resolution), "desktop");
        fprintf(file, "[graphics]\nrenderer = %d\nresolution = %s\nresolution_scale = %d\ntexture_filter = %d\nanisotropy = %d\nvsync = %d\nunlock_frame_rate = %d\nantialiasing = %d\ntexture_quality = %d\nfov = %d\nads_fov = %d\nmodel_detail = %d\nshadow_quality = %d\naspect_ratio = %d\nconsole_depth = %d\nnative_commands = %d\nwatch_writes = %d\nwindow_mode = %d\nfps_overlay = %d\nstats_overlay = %d\n",
            v.renderer, resolution, v.resolutionScale, v.textureFilter, v.anisotropy, v.vsync ? 1 : 0, v.unlockFrameRate ? 1 : 0, v.antialiasing, v.textureQuality, v.fov, v.adsFov, v.modelDetail, v.shadowQuality, v.aspectRatio, v.consoleDepth ? 1 : 0, v.nativeCommands ? 1 : 0, v.watchWrites ? 1 : 0, v.windowMode, v.fpsOverlay ? 1 : 0, v.statsOverlay ? 1 : 0);
        fprintf(file, "sharp_shadows = %d\neffects_resolution = %d\nfoliage_coverage = %d\nfast_shadow_maps = %d\n", v.sharpShadows ? 1 : 0,
                v.effectsResolution, v.foliageCoverage ? 1 : 0, v.fastShadowMaps ? 1 : 0);
        fprintf(file, "film_video_levels = %d\nsteady_decals = %d\n", v.filmVideoLevels ? 1 : 0, v.steadyDecals ? 1 : 0);
        fprintf(file, "opt_fast_constants = %d\nopt_shader_load_cache = %d\nopt_strong_change_check = %d\nopt_range_looks = %d\nopt_opaque_shaders = %d\nopt_staging_uploads = %d\nopt_texture_pool = %d\n",
                v.fastConstants ? 1 : 0, v.shaderLoadCache ? 1 : 0, v.strongChangeCheck ? 1 : 0,
                v.rangeLooks ? 1 : 0, v.opaqueShaders ? 1 : 0, v.stagingUploads ? 1 : 0, v.texturePool ? 1 : 0);
        fprintf(file, "hdr = %d\nhdr_tone_mapper = %d\nhdr_peak_nits = %.0f\nhdr_game_nits = %.0f\nhdr_ui_nits = %.0f\nhdr_gamma_correction = %d\n",
                v.hdr ? 1 : 0, v.hdrToneMapper, v.hdrPeakNits, v.hdrGameNits, v.hdrUiNits, v.hdrGammaCorrection);
        fprintf(file, "hdr_exposure = %.2f\nhdr_highlights = %.0f\nhdr_shadows = %.0f\nhdr_contrast = %.0f\nhdr_saturation = %.0f\nhdr_highlight_saturation = %.0f\nhdr_blowout = %.0f\nhdr_flare = %.0f\nhdr_hue_correction = %.0f\nhdr_cone_response = %.0f\nhdr_bloom = %.0f\nhdr_tv_ramp = %.0f\nhdr_film_upgrade = %d\nhdr_hud_match = %d\n",
                v.hdrExposure, v.hdrHighlights, v.hdrShadows, v.hdrContrast, v.hdrSaturation, v.hdrHighlightSaturation, v.hdrBlowout,
                v.hdrFlare, v.hdrHueCorrection, v.hdrConeResponse, v.hdrBloom, v.hdrTvRamp, v.hdrFilmUpgrade ? 1 : 0, v.hdrHudMatch ? 1 : 0);
        fprintf(file, "frame_cap = %d\ntexture_dump = %d\ntexture_replace = %d\n", v.frameCap, v.textureDump ? 1 : 0, v.textureReplace ? 1 : 0);
        fprintf(file, "[game]\naim_assist = %d\nsubtitles = %d\nconsole = %d\ncontroller = %d\nbutton_prompts = %d\nlook_sensitivity = %.2f\nraw_mouse = %d\nauto_update = %d\n", v.aimAssist ? 1 : 0,
            v.subtitles ? 1 : 0, v.console ? 1 : 0, v.controller ? 1 : 0, v.buttonPrompts, v.mouseSensitivity, v.rawMouse ? 1 : 0, v.autoUpdate ? 1 : 0);
        fprintf(file, "aim_toggle = %d\nads_sensitivity = %.2f\naim_blur = %d\n", v.aimToggle ? 1 : 0, v.adsSensitivity, v.aimBlur ? 1 : 0);
        fprintf(file, "text_language = %s\n", v.textLanguage == 1 ? "hu" : "en");
        fprintf(file, "master_volume = %d\naudio_channels = %d\n", v.masterVolume, v.audioChannels);
        fprintf(file, "[keys]\n");
        for (int i = 0; i < ActionCount; i++) fprintf(file, "key_%s = %d\n", ActionKeys[i], v.keys[i]);
        fclose(file);
    }
}

void Settings::Resolution(const Values& values, int& width, int& height)
{
    if (values.resolutionWidth > 0 && values.resolutionHeight > 0)
    {
        width = values.resolutionWidth;
        height = values.resolutionHeight;
        return;
    }
    width = GetSystemMetrics(SM_CXSCREEN);
    height = GetSystemMetrics(SM_CYSCREEN);
    if (width <= 0 || height <= 0) { width = 1280; height = 720; }
}

void Settings::DrawnSize(const Values& values, int& width, int& height)
{
    Resolution(values, width, height);
    const int percent = values.resolutionScale < 25 ? 25 : values.resolutionScale > 200 ? 200 : values.resolutionScale;
    width = std::max(320, width * percent / 100);
    height = std::max(240, height * percent / 100);
}

const std::vector<Settings::Mode>& Settings::DisplayModes()
{
    // Asked of the display once: every size it offers at 720p or better,
    // each once, smallest first, the desktop's among them.
    static const std::vector<Mode> modes = []() {
        std::vector<Mode> found;
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        for (DWORD i = 0; EnumDisplaySettingsW(nullptr, i, &mode); i++)
        {
            if (mode.dmPelsWidth < 1280 || mode.dmPelsHeight < 720) continue;
            bool known = false;
            for (const Mode& m : found) if (m.width == int(mode.dmPelsWidth) && m.height == int(mode.dmPelsHeight)) known = true;
            if (!known) found.push_back({ int(mode.dmPelsWidth), int(mode.dmPelsHeight) });
        }
        const Mode desktop{ GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
        bool known = false;
        for (const Mode& m : found) if (m.width == desktop.width && m.height == desktop.height) known = true;
        if (!known && desktop.width > 0) found.push_back(desktop);
        std::sort(found.begin(), found.end(), [](const Mode& a, const Mode& b) { return a.width * a.height < b.width * b.height || (a.width * a.height == b.width * b.height && a.width < b.width); });
        return found;
    }();
    return modes;
}

// The model detail: COD3_MODELDETAIL=0|1 over the settings.
static int ModelDetail(const Settings::Values& v)
{
    static const int detailOverride = []() { const char* t = getenv("COD3_MODELDETAIL"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
    return detailOverride >= 0 ? detailOverride : v.modelDetail;
}

std::string Settings::TitleCommandsChanged()
{
    // What the title has, to begin with, is its own: the field of view 65,
    // the aim assist on, r_lodscale 1. So the first ask brings whatever the
    // settings change from that, and later ones what changed since. "set"
    // rather than the variable's name alone, because the first ask comes
    // before the title has registered most of them (the renderer's and the
    // game's come later): set makes the variable, and the title's own
    // registration then keeps the value it finds. These are commands on
    // the title's buffer, not lines in a config: the title never runs
    // default.cfg (it only checks the file is there), so lines added to
    // it did nothing.
    static int appliedFov = 65, appliedDetail = 0;
    static bool appliedAimAssist = true, appliedSubtitles = true;
    const Values v = Get();
    std::string commands;
    if (v.fov != appliedFov)
    {
        appliedFov = v.fov;
        commands += "set cg_fov " + std::to_string(v.fov) + "\n";
    }
    // The model detail is the title's r_lodscale, which scales the distance
    // at which a model steps down to a coarser version: 1 is its own, 0
    // keeps the finest one at every distance.
    if (ModelDetail(v) != appliedDetail)
    {
        appliedDetail = ModelDetail(v);
        commands += std::string("set r_lodscale ") + (appliedDetail ? "0" : "1") + "\n";
    }
    // The title's own aim assist is three switches of its console.
    if (v.aimAssist != appliedAimAssist)
    {
        appliedAimAssist = v.aimAssist;
        const char* on = v.aimAssist ? "1" : "0";
        commands += std::string("set aim_slowdown_enabled ") + on + "\nset aim_lockon_enabled " + on + "\nset aim_autoaim_enabled " + on + "\n";
    }
    // The subtitles: the title's cg_subtitles (its dvar table at 0x825CE180
    // gives it "1"), the same switch as its own Gameplay page.
    if (v.subtitles != appliedSubtitles)
    {
        appliedSubtitles = v.subtitles;
        commands += std::string("set cg_subtitles ") + (v.subtitles ? "1" : "0") + "\n";
    }
    if (!commands.empty()) commands.pop_back();   // the last line's end: the queue adds its own
    return commands;
}
