#include "../CoD3Host/settings.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

static void Check(bool pass, const char* message)
{
    if (!pass) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2) throw std::runtime_error("Pass an isolated test directory");
        const auto folder = std::filesystem::path(argv[1]) / "av-settings";
        std::filesystem::create_directories(folder);
        std::ofstream(folder / "CoD3Recomp.ini") << "text_language = hu\naim_toggle = 1\nads_sensitivity = 0.65\n";
        Settings::Load(folder, true);
        auto v = Settings::Get();
        Check(v.masterVolume == 100 && v.audioChannels == 0 && !v.hdr && !v.textureDump, "Old INI migration changed defaults");
        v.hdr = true; v.hdrToneMapper = 7; v.hdrPeakNits = 1200; v.hdrGameNits = 250;
        v.hdrUiNits = 180; v.hdrGammaCorrection = 2; v.hdrExposure = 1.25f;
        v.hdrHighlights = 61; v.hdrShadows = 43; v.hdrContrast = 55; v.hdrSaturation = 60;
        v.hdrHighlightSaturation = 39; v.hdrBlowout = 17; v.hdrFlare = 9;
        v.hdrHueCorrection = 80; v.hdrConeResponse = 42; v.hdrBloom = 72; v.hdrTvRamp = 1;
        v.hdrHudMatch = false; v.hdrFilmUpgrade = true;
        v.frameCap = 165; v.textureDump = true; v.textureReplace = false;
        v.sharpShadows = false; v.effectsResolution = 2; v.foliageCoverage = false;
        v.fastShadowMaps = false; v.steadyDecals = false; v.filmVideoLevels = false;
        v.fastConstants = false; v.shaderLoadCache = false; v.strongChangeCheck = false;
        v.rangeLooks = false; v.opaqueShaders = false; v.stagingUploads = false; v.texturePool = false;
        v.masterVolume = 37; v.audioChannels = 2;
        Settings::Set(v);
        Settings::Load(folder, true);
        const auto restored = Settings::Get();
        const bool Settings::Values::* flags[] = { &Settings::Values::hdr, &Settings::Values::textureDump,
            &Settings::Values::textureReplace, &Settings::Values::sharpShadows, &Settings::Values::foliageCoverage,
            &Settings::Values::fastShadowMaps, &Settings::Values::steadyDecals, &Settings::Values::filmVideoLevels,
            &Settings::Values::fastConstants, &Settings::Values::shaderLoadCache, &Settings::Values::strongChangeCheck,
            &Settings::Values::rangeLooks, &Settings::Values::opaqueShaders, &Settings::Values::stagingUploads,
            &Settings::Values::texturePool, &Settings::Values::hdrHudMatch, &Settings::Values::hdrFilmUpgrade };
        for (auto flag : flags) Check(restored.*flag == v.*flag, "An A/V toggle was lost");
        const float Settings::Values::* sliders[] = { &Settings::Values::hdrPeakNits, &Settings::Values::hdrGameNits,
            &Settings::Values::hdrUiNits, &Settings::Values::hdrExposure, &Settings::Values::hdrHighlights,
            &Settings::Values::hdrShadows, &Settings::Values::hdrContrast, &Settings::Values::hdrSaturation,
            &Settings::Values::hdrHighlightSaturation, &Settings::Values::hdrBlowout, &Settings::Values::hdrFlare,
            &Settings::Values::hdrHueCorrection, &Settings::Values::hdrConeResponse, &Settings::Values::hdrBloom,
            &Settings::Values::hdrTvRamp };
        for (auto slider : sliders) Check(restored.*slider == v.*slider, "An HDR slider was lost");
        Check(restored.frameCap == 165 && restored.masterVolume == 37 && restored.audioChannels == 2 &&
              restored.effectsResolution == 2 && restored.hdrToneMapper == 7 && restored.hdrGammaCorrection == 2, "An A/V selection was lost");
        Check(restored.textLanguage == 1 && restored.aimToggle && restored.adsSensitivity == 0.65f, "Local language/input settings changed");
        Settings::Preview(Settings::Defaults());
        Settings::Load(folder, true);
        Check(Settings::Get().hdr && Settings::Get().masterVolume == 37, "Preview unexpectedly wrote the INI");
        std::ofstream(folder / "CoD3Recomp.ini") << "frame_cap = -20\naudio_channels = 99\nmaster_volume = 999\neffects_resolution = 8\nhdr_tone_mapper = 99\nhdr_exposure = 99\n";
        Settings::Load(folder, true);
        v = Settings::Get();
        Check(v.frameCap == 0 && v.audioChannels == 2 && v.masterVolume == 100 && v.effectsResolution == 0 &&
              v.hdrToneMapper == 7 && v.hdrExposure == 4.0f, "Invalid A/V settings escaped their limits");
        std::cout << "PASS: A/V migration, persistence, HDR controls, preview, bounds and existing language/input settings\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
