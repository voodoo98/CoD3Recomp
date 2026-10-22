#include "localization.h"

#include <algorithm>
#include <cstdio>
#include <cwctype>
#include <string_view>

namespace fs = std::filesystem;

namespace
{
    bool g_hungarian = false;
    int g_language = Localization::English;
    fs::path g_gameRoot;

    bool File(const fs::path& path)
    {
        std::error_code error;
        return fs::is_regular_file(path, error);
    }

    char WeaponSuffixVowel(std::string_view name)
    {
        // Ignore the game's colour escapes and typography, keep the spoken name.
        std::string word;
        for (size_t i = 0; i < name.size(); ++i)
        {
            const unsigned char c = static_cast<unsigned char>(name[i]);
            if (c == '^' && i + 1 < name.size() && name[i + 1] >= '0' && name[i + 1] <= '9') { ++i; continue; }
            if (c == ' ' || c == '\t') { if (!word.empty() && word.back() != ' ') word += ' '; continue; }
            if (c == '-' || c == '.' || c == '"') continue;
            word += char(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
        }
        while (!word.empty() && word.back() == ' ') word.pop_back();
        // Numbered models follow their spoken number: negyven -> re,
        // harmincnyolc -> ra. A final letter remains part of the name (Kar98k).
        if (!word.empty() && word.back() >= '0' && word.back() <= '9')
        {
            unsigned ending = unsigned(word.back() - '0');
            if (ending == 0 && word.size() > 1 && word[word.size() - 2] >= '0' && word[word.size() - 2] <= '9')
            {
                ending = unsigned(word[word.size() - 2] - '0');
                return ending == 1 || ending == 4 || ending == 5 || ending == 7 || ending == 9 ? 'e' : 'a';
            }
            return ending == 1 || ending == 2 || ending == 4 || ending == 5 || ending == 7 || ending == 9 ? 'e' : 'a';
        }
        // Foreign names/compounds whose suffix is not determined by all their
        // written vowels. Other translated labels follow their last word.
        if (word.ends_with("panzerschreck") || word.ends_with("luger")) return 'e';
        const size_t last = word.rfind(' ');
        const std::string_view tail = std::string_view(word).substr(last == std::string::npos ? 0 : last + 1);
        // Guest text uses the Hungarian Windows-1252 font slots.
        for (unsigned char c : tail)
            if (c == 'a' || c == 'o' || c == 'u' || c == 0xE1 || c == 0xC1 ||
                c == 0xF3 || c == 0xD3 || c == 0xFA || c == 0xDA) return 'a';
        return 'e';
    }
}

bool Localization::HasHungarian(const fs::path& gameRoot)
{
    return File(gameRoot / "hu_install.json") &&
        File(gameRoot / "sp/global.cod") && File(gameRoot / "sp/global.cod.orig") &&
        File(gameRoot / "sp/frontend.cod") && File(gameRoot / "sp/frontend.cod.orig");
}

void Localization::Initialize(const fs::path& gameRoot, int language)
{
    g_hungarian = HasHungarian(gameRoot);
    g_gameRoot = gameRoot.lexically_normal();
    g_language = g_hungarian && language == Hungarian ? Hungarian : English;
    printf("localization: text language %s (changes apply after restart)\n",
        g_language == Hungarian ? "Magyar" : "English");
}

bool Localization::HasHungarian() { return g_hungarian; }
int Localization::ActiveLanguage() { return g_language; }

bool Localization::FixWeaponSwapPrompt(std::string& text)
{
    if (g_language != Hungarian || (!text.starts_with("Tartsd nyomva ") && !text.starts_with("[HOLD_USE]-t a "))) return false;
    const size_t swap = text.rfind(" csere ");
    if (swap == std::string::npos) return false;
    size_t end = text.size();
    while (end && (text[end - 1] == ' ' || text[end - 1] == '\n' || text[end - 1] == '\r' || text[end - 1] == '.')) --end;
    const size_t start = swap + 7;
    if (end < start + 4 || text[end - 3] != '-' || text[end - 2] != 'r' ||
        (text[end - 1] != 'a' && text[end - 1] != 'e')) return false;
    const char vowel = WeaponSuffixVowel(std::string_view(text).substr(start, end - 3 - start));
    if (text[end - 1] == vowel) return false;
    text[end - 1] = vowel;
    return true;
}

fs::path Localization::ReadPath(const fs::path& installedPath)
{
    if (g_language != English || !g_hungarian) return installedPath;
    const fs::path relative = installedPath.lexically_normal().lexically_relative(g_gameRoot);
    if (relative.empty() || *relative.begin() == L"..") return installedPath;
    std::wstring extension = installedPath.extension().wstring();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    if (extension != L".cod") return installedPath;
    fs::path original = installedPath;
    original += L".orig";
    // Only translated archives have an original backup. Other disc assets
    // already contain their original English data and use their usual path.
    return File(original) ? original : installedPath;
}
