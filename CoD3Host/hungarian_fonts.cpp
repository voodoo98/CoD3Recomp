// Four Hungarian double-acute letters in the title's own bitmap fonts.
// The translation pack uses the CP1250 byte slots D5/F5/DB/FB for O/o/U/u
// with double acute, while retaining CP1252 for all other text. This is
// enabled only by the matching localization manifest (or a diagnostic run).
#include "kernel.h"
#include "hungarian_font_data.h"
#include "hungarian_fonts.h"
#include "localization.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <mutex>
#include <string>

namespace
{
    bool Enabled()
    {
        static const bool enabled = [] {
            if (getenv("COD3_HU_FONT_TEST") != nullptr) return true;
            if (Localization::ActiveLanguage() != Localization::Hungarian) return false;
            std::ifstream file(Guest::GameRoot / "hu_install.json", std::ios::binary);
            const std::string manifest((std::istreambuf_iterator<char>(file)), {});
            return manifest.find("\"hu-double-acute-v1\"") != std::string::npos;
        }();
        return enabled;
    }

    bool Physical(uint32_t address, uint32_t size)
    {
        return address >= 0xA0000000u && size < 0x20000000u &&
            uint64_t(address & 0x1FFFFFFFu) + size <= 0x20000000ull;
    }

    template<size_t N, size_t M>
    void Patch(uint8_t* base, uint32_t font, const HungarianFontData::Block (&blocks)[N],
        const HungarianFontData::Block (&mips)[M], const char* name)
    {
        if (Guest::Read32(base, font + 36) != 32 || Guest::Read32(base, font + 40) != 224) return;
        const uint32_t metrics = Guest::Read32(base, font + 8);
        const uint32_t uv = Guest::Read32(base, font + 44);
        const uint32_t texture = Guest::Read32(base, font + 4);
        if (!Physical(metrics, 224 * 28) || !Physical(uv, 224 * 16) || !Physical(texture, 96)) return;

        const uint32_t fetch0 = Guest::Read32(base, texture + 72);
        const uint32_t fetch1 = Guest::Read32(base, texture + 76);
        const uint32_t fetch2 = Guest::Read32(base, texture + 80);
        // These deltas are for this version's 512 x 256, tiled BC3 atlases.
        // Refuse a different asset instead of applying offsets to an unknown font.
        if ((fetch0 & 0xFFC00000u) != 0x84000000u || (fetch1 & 0xFFu) != 0x54 || fetch2 != 0x001FE1FFu) return;
        const uint32_t pixels = Guest::PhysicalAlias(fetch1 & 0xFFFFF000u);
        const uint32_t mipPixels = Guest::PhysicalAlias(Guest::Read32(base, texture + 92) & 0xFFFFF000u);
        if (!Physical(pixels, 512 * 256) || !Physical(mipPixels, 65536)) return;
        constexpr uint32_t letters[][2] = { {0xD5, 0xD2}, {0xF5, 0xF2}, {0xDB, 0xD9}, {0xFB, 0xF9} };

        // Lookups may come from different title threads. Check every original
        // block before changing any, then publish the new metrics and UVs last.
        static std::mutex mutex;
        std::lock_guard<std::mutex> lock(mutex);
        bool alreadyPatched = true;
        for (const auto& pair : letters)
            alreadyPatched &= memcmp(base + metrics + (pair[0] - 32) * 28,
                base + metrics + (pair[1] - 32) * 28, 28) == 0;
        if (alreadyPatched && memcmp(base + pixels + blocks[0].offset, blocks[0].after, 16) == 0 &&
            memcmp(base + mipPixels + mips[0].offset, mips[0].after, 16) == 0) return;
        for (const auto& block : blocks)
            if (memcmp(base + pixels + block.offset, block.before, 16) != 0) return;
        for (const auto& block : mips)
            if (memcmp(base + mipPixels + block.offset, block.before, 16) != 0) return;

        for (const auto& block : blocks) memcpy(base + pixels + block.offset, block.after, 16);
        for (const auto& block : mips) memcpy(base + mipPixels + block.offset, block.after, 16);
        for (const auto& pair : letters)
        {
            memcpy(base + metrics + (pair[0] - 32) * 28, base + metrics + (pair[1] - 32) * 28, 28);
            memcpy(base + uv + (pair[0] - 32) * 16, base + uv + (pair[1] - 32) * 16, 16);
        }
        printf("localization: Hungarian double acute glyphs ready in %s\n", name);
        fflush(stdout);
    }

    void PatchFont(uint8_t* base, uint32_t font)
    {
        if (!Physical(font, 56)) return;
        const uint32_t name = Guest::Read32(base, font);
        if (!Physical(name, 32)) return;
        if (memcmp(base + name + 4, "garamond\0", 9) == 0)
            Patch(base, font, HungarianFontData::garamond, HungarianFontData::garamond_mips, "garamond");
        else if (memcmp(base + name + 4, "i_helvetica_bold\0", 17) == 0)
            Patch(base, font, HungarianFontData::i_helvetica_bold, HungarianFontData::i_helvetica_bold_mips, "i_helvetica_bold");
        else if (memcmp(base + name + 4, "i_helvetica_bold_italic\0", 24) == 0)
            Patch(base, font, HungarianFontData::i_helvetica_bold_italic, HungarianFontData::i_helvetica_bold_italic_mips, "i_helvetica_bold_italic");
    }
}

// Some loaded fonts are obtained directly from cached bank entries, without
// AssetBank::Find. Check the small bank table at the title's frame boundary as
// well; the byte guards make this idempotent and handle a bank reloaded into
// the same guest memory after a mission change.
void HungarianFonts::Update()
{
    if (!Enabled()) return;
    uint8_t* base = Guest::Base;
    const uint32_t manager = Guest::Read32(base, 0x82A2AB68u);
    if (manager != 0x826E3E00u) return;
    for (uint32_t id = 0; id < 100; id++)
    {
        const uint32_t bank = Guest::Read32(base, manager + 52 + id * 4);
        if (!Physical(bank, 20) || Guest::Read32(base, bank) != 0x49494253u || Guest::Read32(base, bank + 8) != 13) continue;
        const uint32_t types = Guest::Read32(base, bank + 12);
        if (!Physical(types, 13 * 32) || Guest::Read32(base, types + 32) != 1) continue;
        const uint32_t count = Guest::Read32(base, types + 56);
        const uint32_t entries = Guest::Read32(base, types + 60);
        if (count > 16 || !Physical(entries, count * 8)) continue;
        for (uint32_t i = 0; i < count; i++) PatchFont(base, Guest::Read32(base, entries + i * 8 + 4));
    }
}

// AssetBank::Find returns the address of the resource pointer, rather than
// the resource itself. Type 1 is FONT. Texture and button icon lookups remain
// unchanged, as do every font and every character when English is installed.
extern "C" PPC_FUNC(__imp__sub_820C0168);
PPC_FUNC(sub_820C0168)
{
    const uint32_t type = ctx.r4.u32;
    __imp__sub_820C0168(ctx, base);
    if (type == 1 && ctx.r3.u32 != 0 && Enabled())
        PatchFont(base, Guest::Read32(base, ctx.r3.u32));
}
