// The player's settings: what the F11 menu shows and changes, kept in
// CoD3Recomp.ini beside the executable. Read from any thread through a copy;
// written by the menu.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Settings
{
    // The keys the menu binds, each standing in for one of the pad's
    // controls. The order is the order the menu lists them in.
    enum Action
    {
        Fire, Aim, Jump, Crouch, Use, Reload, Weapon, Melee, Sprint, Binoculars, Grenade, Smoke,
        Objectives, Pause, Forward, Backward, Left, Right,
        ActionCount
    };

    const char* ActionName(Action action);   // "Fire", "Aim", ...

    // A key, as a Windows virtual key code, or 0 for none.
    const char* KeyName(int virtualKey);

    struct Values
    {
        int renderer = 0;             // 0 Direct3D 11; 1 and 2 are names only
        int resolutionWidth = 0;      // the resolution the frame is drawn at; 0 by 0 is the desktop's
        int resolutionHeight = 0;
        int resolutionScale = 100;    // per cent of it the frame is really drawn at, 25 .. 200
        int textureFilter = 3;        // 0 the title's own, 1 bilinear, 2 trilinear, 3 anisotropic
        int anisotropy = 16;          // 2..16, with textureFilter 3
        bool vsync = true;
        bool unlockFrameRate = true;  // a level played at what the machine can do; the menus and films keep the console's 60
        int frameCap = 0;             // evenly paced level frames, 0 for no cap
        bool textureDump = false;     // write textures to textures/dump
        bool textureReplace = true;   // load matching PNG/DDS files from textures/replace
        int antialiasing = 3;         // 0 none, 1 FXAA, 2 MSAA 2x, 3 MSAA 4x, 4 MSAA 8x, 5 MSAA 4x and FXAA
        int textureQuality = 2;       // 0 low (two mip levels dropped), 1 medium (one), 2 high (all)
        int fov = 65;                 // the title's cg_fov, 65 (its own) .. 100
        int adsFov = 100;             // per cent of each weapon's own adsZoomFov when aiming down the sights, 100 (its own) .. 250; scopes are left alone
        int modelDetail = 1;          // 0 the title's own level of detail (r_lodscale 1), 1 the most detailed models at every distance (r_lodscale 0)
        bool fog = true;              // the title's fog, r_fog
        int drawDistance = 0;         // 0 automatic; 1 far, 2 medium, 3 near (r_zfar)
        int effects = 0;              // 0 all, 1 fewer distant effects, 2 off
        bool dynamicLights = true;    // muzzle flashes and explosions lighting nearby surfaces
        int aspectRatio = 0;          // the picture's shape in a level: 0 the console's 16:9, 1 the window's, 2 21:9, 3 32:9 (widescreen.cpp)
        int shadowQuality = 0;        // the shadow maps' size: 0 as the frame (the console's 512 times the frame's scale), 1 512, 2 1024, 3 2048, 4 4096 (render_resources.cpp)
        bool aimBlur = false;          // depth of field while aiming down the sights
        bool consoleDepth = true;     // depth cut to the console's 24 bit float precision (no flickering surfaces; costs with MSAA)
        bool sharpShadows = true;     // the shadows' four tap filtering on the shadow map's own texels, point sampled (render_commands.cpp)
        int effectsResolution = 0;    // the half size surfaces (smoke, soft particles, shadow cascades): 0 the console's half size, 1 three quarters (1.5x), 2 full size (2x) (render_resources.cpp)
        bool foliageCoverage = true;  // with MSAA, alpha tested edges (foliage, fences) spread over the samples (xenos_hlsl.cpp)
        bool fastShadowMaps = true;   // the shadow maps' casters drawn with no pixel program, the GPU's depth only path (render_commands.cpp)
        bool steadyDecals = true;     // the decals and other layers over a surface put a little nearer, so they do not flicker (render_pipeline.cpp)
        bool filmVideoLevels = true;  // the films shown as a video player shows them, without the display's TV gamma ramp (render_d3d11.cpp)
        // The optimizations, each of which can be turned off to find one
        // behind something that looks wrong (tuning.h).
        bool fastConstants = true;      // the constant packets stored as runs
        bool shaderLoadCache = true;    // programs loaded again matched without hashing them byte by byte
        bool strongChangeCheck = true;  // the eight lane fingerprint for telling whether memory changed
        bool rangeLooks = true;         // with "Watch memory writes" off, the vertices each draw reads of a changing big buffer looked at again
        bool opaqueShaders = true;      // pixel programs without the alpha test for the draws that do not test
        bool stagingUploads = true;     // textures sent to the GPU through staging copies
        bool texturePool = true;        // textures of a shape kept and used again
        // The HDR mode (F11's HDR tab; render_d3d11.cpp, hdr/cod3_hdr.hlsl on
        // RenoDX): an scRGB swap chain, the frame's targets 16 bit float, and
        // RenoDX's tone mappers and sliders. The sliders are RenoDX's own
        // scales: 50 is the game's look.
        bool hdr = false;
        int hdrToneMapper = 3;          // 0 vanilla (clipped as SDR), 1 none, 2 ACES, 3 RenoDRT, 4 PsychoV 17, 5 PsychoV 22, 6 PsychoV 30, 7 Prism
        float hdrPeakNits = 0.0f;       // the display's peak; 0 is what Windows reports for it (1000 when it reports nothing)
        float hdrGameNits = 203.0f;     // the game's white
        float hdrUiNits = 203.0f;       // the HUD's and the menus' white
        int hdrGammaCorrection = 1;     // the SDR picture decoded as 0 sRGB, 1 gamma 2.2, 2 gamma 2.4
        float hdrExposure = 1.0f;       // 0 .. 4
        float hdrHighlights = 50.0f;    // 0 .. 100
        float hdrShadows = 50.0f;
        float hdrContrast = 50.0f;
        float hdrSaturation = 50.0f;
        float hdrHighlightSaturation = 50.0f;   // RenoDRT
        float hdrBlowout = 0.0f;                // RenoDRT
        float hdrFlare = 0.0f;                  // RenoDRT
        float hdrHueCorrection = 100.0f;
        float hdrConeResponse = 50.0f;          // PsychoV
        float hdrBloom = 50.0f;         // the bloom's strength (in SDR too): 50 the game's
        float hdrTvRamp = 100.0f;       // how much of the title's TV gamma ramp, 0 .. 100: 100 the table's full shape, held to Game brightness (mid grey and white kept: cod3_hdr.hlsl's TvRampAnchored), lower lightens the shadows
        bool hdrHudMatch = true;        // the HUD's white (the title draws it at about 86%) brought to the UI brightness
        bool hdrFilmUpgrade = false;    // films expanded to HDR (RenoDX's video upscale)
        bool nativeCommands = true;   // the draws' state taken from the game's own D3D calls, the command stream run only for what the GPU still does (docs/native-d3d11-plan.md, N3-N4; on by default since all fifteen levels ran on it; after a restart)
        bool watchWrites = true;      // still textures and vertex buffers known unchanged by their pages, not hashed every frame (write_watch.h; on by default; after a restart)
        int windowMode = 1;           // 0 windowed, 1 borderless full screen
        bool fpsOverlay = false;
        bool statsOverlay = false;    // the renderer's counters over the picture
        bool aimAssist = true;        // the title's own, through its config
        bool subtitles = true;        // the title's cg_subtitles (on by default there too)
        int textLanguage = 0;          // 0 English, 1 Magyar; all game text, after restart
        bool console = false;         // the key left of 1 opens the console (the host's: the title's own was left out of this build)
        bool controller = true;       // read a pad when one is there
        int masterVolume = 100;       // the output volume, per cent; before the limiter
        int audioChannels = 0;        // 0 automatic, 1 stereo, 2 5.1 surround
        int buttonPrompts = 0;        // the prompts' buttons: 0 as the last used (pad or keyboard and mouse), 1 the pad's, 2 the keys (pc_prompts.cpp)
        float mouseSensitivity = 1.0f;   // the look's, both axes, 0.05 .. 5: 1 is 0.066 degrees a count (mouse_look.cpp); look_sensitivity in the ini
        float adsSensitivity = 1.5f;     // the look's gain while aiming down the sights, times the mouse sensitivity
        bool aimToggle = false;          // aiming down the sights on a press, not while held
        bool rawMouse = true;            // the mouse's own counts, not the pointer's travel
        bool autoUpdate = true;          // on start, a newer release from GitHub downloaded and started (updater.cpp)
        int keys[ActionCount] = {};
    };

    Values Defaults();

    // The resolution chosen, resolved: the desktop's when the setting is
    // 0 by 0. In a window the window is made this size; over the whole
    // screen the picture is scaled to it.
    void Resolution(const Values& values, int& width, int& height);

    // The size the frame is really drawn at: the resolution times the scale.
    void DrawnSize(const Values& values, int& width, int& height);

    // The resolutions the display offers, largest last, for the menu.
    struct Mode { int width, height; };
    const std::vector<Mode>& DisplayModes();

    // The current values, and setting them (which also saves).
    Values Get();
    void Set(const Values& values);
    // Setting them without saving, for a slider dragged in the menu (the
    // HDR tabs apply as they move); Save() when the drag is over.
    void Preview(const Values& values);

    // Reads the file beside the executable, or starts from the defaults.
    void Load(const std::filesystem::path& exeDirectory, bool hungarianAvailable = false);
    void Save();

    // The console commands the settings ask of the title (the field of
    // view, the model detail, the aim assist, the subtitles), for its command buffer: at
    // the first ask whatever differs from the title's own values, then
    // what changed since the last ask. Empty when there is nothing to say.
    std::string TitleCommandsChanged();
}
