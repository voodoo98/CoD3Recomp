#include "localization.h"
#include "settings.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs = std::filesystem;
void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void Write(const fs::path& p, const char* s) { fs::create_directories(p.parent_path()); std::ofstream(p) << s; }
int main(int argc, char** argv)
{
    if (argc != 2) { std::cerr << "Usage: localization-tests <test-output-directory>\n"; return 2; }
    try
    {
        const fs::path workspace = fs::absolute(argv[1]);
        const fs::path root = workspace / "fixtures";
        const fs::path game = root / "game";
        const fs::path ini = root / "settings";
        fs::create_directories(ini);
        Write(game / "hu_install.json", "{}");
        for (const char* name : { "global.cod", "frontend.cod" })
        {
            Write(game / "sp" / name, "Hungarian archive");
            Write(game / "sp" / (std::string(name) + ".orig"), "Original English archive");
        }
        Write(game / "sp/anim.cod", "Untranslated archive");
        Write(game / "sp/Global.wbk", "Audio");
        Write(root / "saves/sample.cod", "Save data");
        Write(root / "saves/sample.cod.orig", "Save backup");
        Write(ini / "CoD3Recomp.ini", "[graphics]\nfov = 90\n[game]\nsubtitles = 0\n[keys]\nkey_jump = 74\n");
        Check(Localization::HasHungarian(game), "Hungarian pack not detected");
        Settings::Load(ini, true);
        Check(Settings::Get().textLanguage == 1, "Legacy Hungarian installation changed language");
        Localization::Initialize(game, Settings::Get().textLanguage);
        const fs::path archive = game / "sp/global.cod";
        Check(Localization::ReadPath(archive) == archive, "Hungarian read routed to original");
        // Swap targets are dynamic, and model numbers follow their pronunciation.
        // Exercise both old fixed suffixes and preserve the source weapon/key.
        const std::pair<const char*, const char*> weapons[] = {
            {"Kar98k", "-ra"}, {"KAR98K", "-ra"}, {"MP40", "-re"}, {"MP44", "-re"},
            {"MG34", "-re"}, {"P38", "-ra"}, {"Gewehr 43", "-ra"}, {"Panzerfaust 60", "-ra"},
            {"M1 Garand", "-ra"}, {"Thompson", "-ra"}, {"M1 karab\xE9ly", "-ra"},
            {"Springfield", "-re"}, {"Panzerschreck", "-re"}, {"^2MP40^7", "-re"},
            {"Beretta M1934", "-re"}, {"Breda M1930", "-ra"}, {"Tavcsoves K98", "-ra"}
        };
        for (const auto& [weapon, suffix] : weapons)
            for (const char* oldSuffix : {"-ra", "-re"})
              for (const std::string prefix : {std::string("Tartsd nyomva [E]-t a MP40 csere "), std::string("[HOLD_USE]-t a MP40 csere ")})
            {
                std::string text = prefix + weapon + oldSuffix;
                const size_t bytes = text.size();
                Localization::FixWeaponSwapPrompt(text);
                Check(text == prefix + weapon + suffix, "Wrong weapon swap suffix");
                Check(text.size() == bytes, "Swap prompt buffer length changed");
                Check(!Localization::FixWeaponSwapPrompt(text), "Suffix correction not idempotent");
            }
        std::string punctuation = "Tartsd nyomva [E]-t a MP40 csere Kar98k-re.\n";
        Localization::FixWeaponSwapPrompt(punctuation);
        Check(punctuation == "Tartsd nyomva [E]-t a MP40 csere Kar98k-ra.\n", "Prompt punctuation lost");
        for (const char* other : {"MP40-re", "Egy masik csere Kar98k-re", "Tartsd nyomva [E]-t a csere Kar98k-r"})
        {
            std::string text = other;
            Check(!Localization::FixWeaponSwapPrompt(text) && text == other, "Unrelated/truncated text changed");
        }
        auto changed = Settings::Get();
        changed.textLanguage = 0;
        Settings::Set(changed);
        Check(Localization::ActiveLanguage() == 1, "Applying changed active language without restart");
        Check(Localization::ReadPath(archive) == archive, "Later loads would mix languages");
        Settings::Load(ini, true);
        Check(Settings::Get().textLanguage == 0, "English selection did not persist");
        Check(Settings::Get().fov == 90 && !Settings::Get().subtitles && Settings::Get().keys[Settings::Jump] == 74,
            "Unrelated settings changed");
        Localization::Initialize(game, Settings::Get().textLanguage);
        std::string englishIsolation = "Tartsd nyomva [E]-t a MP40 csere Kar98k-re";
        Check(!Localization::FixWeaponSwapPrompt(englishIsolation), "English session rewrote a Hungarian-looking string");
        Check(Localization::ReadPath(archive) == fs::path(archive.string()+".orig"), "English archive not selected");
        Check(Localization::ReadPath(game / "sp/anim.cod") == game / "sp/anim.cod", "Untranslated archive redirected");
        Check(Localization::ReadPath(game / "sp/Global.wbk") == game / "sp/Global.wbk", "Audio redirected");
        Check(Localization::ReadPath(root / "saves/sample.cod") == root / "saves/sample.cod", "Save file redirected");
        changed = Settings::Get(); changed.textLanguage = 1; Settings::Set(changed);
        Settings::Load(ini, true);
        Check(Settings::Get().textLanguage == 1, "Hungarian selection did not persist");
        Settings::Load(ini, false);
        Check(Settings::Get().textLanguage == 0, "Unavailable Hungarian language remained selected");
        Localization::Initialize(root / "no-pack", 1);
        Check(Localization::ActiveLanguage() == 0 && !Localization::HasHungarian(), "Missing pack fallback failed");
        Write(ini / "CoD3Recomp.ini", "text_language = invalid\n");
        Settings::Load(ini, true);
        Check(Settings::Get().textLanguage == 0, "Invalid language did not fall back to English");
        std::cout << "PASS: weapon suffixes, migration, persistence, restart isolation, both languages, archive/audio/save routing, missing pack and invalid setting\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
