#pragma once

#include <filesystem>
#include <string>

namespace Localization
{
    enum Language { English = 0, Hungarian = 1 };

    bool HasHungarian(const std::filesystem::path& gameRoot);
    void Initialize(const std::filesystem::path& gameRoot, int language);
    bool HasHungarian();
    int ActiveLanguage();

    // The weapon name is only known after the guest formats a swap prompt.
    // Correct its -ra/-re suffix in place, without changing the buffer length.
    bool FixWeaponSwapPrompt(std::string& text);

    // Read-only disc paths. The selected language stays fixed for this run,
    // including later level loads, until the player restarts the game.
    std::filesystem::path ReadPath(const std::filesystem::path& installedPath);
}
