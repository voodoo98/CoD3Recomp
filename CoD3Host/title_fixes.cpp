// Fixes to the title's own code: faults of the game itself that the
// console survives by the luck of its memory layout and the port does not.
// Each replaces one recompiled function through its weak alias, which the
// stronger definition here takes the place of, and changes as little as
// the fault needs.

#include "heap_trace.h"
#include "kernel.h"
#include "settings.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

// --- a destructor that runs twice ------------------------------------------------------------------
//
// sub_823F0828 tears down a list of objects at the end of a level (and at a
// mission's restart after "MISSION FAILED", which unloads the level too).
// For each object it calls the destructor, sub_823F04B0, and then deletes
// the object through its vtable; the deleting destructor behind that slot,
// sub_820C8478, calls sub_823F04B0 again. The destructor frees six fields
// and releases their handles without clearing them, so the second run
// frees the same blocks a second time.
//
// The title's allocator is dlmalloc, and a block freed twice goes onto a
// bin's list twice: the list closes into a cycle, and the next allocation
// that walks it (sub_820CCF00) never returns. That was one of the hangs at
// the load of a second level and after a mission's restart. (The console
// runs the same code and does not hang; presumably its memory is laid out
// so that the second free finds the chunk already merged away and is
// refused. That has not been checked.)
//
// The second run is left out: when sub_823F04B0 is called from inside the
// deleting destructor for the very object the teardown loop has just
// destroyed, it returns at once. Every other call runs as it is.
namespace
{
    constexpr uint32_t TeardownLoopCall = 0x823F0880;       // return address of the call in sub_823F0828
    constexpr uint32_t DeletingDestructorCall = 0x820C84A4;  // return address of the call in sub_820C8478
    thread_local uint32_t t_justDestroyed = 0;
}

extern "C" PPC_FUNC(__imp__sub_823F04B0);
PPC_FUNC(sub_823F04B0)
{
    const uint32_t object = ctx.r3.u32;
    const uint32_t from = uint32_t(ctx.lr);
    // COD3_NODTORFIX=1 lets the second run happen, to see whether it still
    // does harm.
    static const bool leaveIt = getenv("COD3_NODTORFIX") != nullptr;
    if (!leaveIt && from == DeletingDestructorCall && object != 0 && object == t_justDestroyed)
    {
        t_justDestroyed = 0;
        return;
    }
    t_justDestroyed = from == TeardownLoopCall ? object : 0;
    HeapTrace::Destroying(ctx, base);
    __imp__sub_823F04B0(ctx, base);
}

// --- the mission's film, skippable once the level has loaded (a PC addition) ---------------------
//
// A mission's film plays while its level loads (sub_824636D0 opens it and
// the loading screen's callback, sub_824EECF8, draws it), and once the load
// is done the level's start (sub_82517A00) plays the rest to its end in
// sub_824EEBA0. That loop asks sub_824D0470 whether to stop: any button
// pressed, once the film's clock (0x82A2A254, from nought in the loop) has
// reached the threshold at 0x82A2A258. The intros set the threshold low and
// can be skipped with A (Space); a mission's film has it set to the
// largest float when it opens, so on the console it plays to its end. Here
// the threshold goes to nought when the loop starts: the load is finished
// by then, and a press skips the rest. COD3_NOFILMSKIP=1 keeps the
// console's way.
extern "C" PPC_FUNC(__imp__sub_824EEBA0);
PPC_FUNC(sub_824EEBA0)
{
    static const bool consoleWay = getenv("COD3_NOFILMSKIP") != nullptr;
    if (!consoleWay) Guest::Write32(base, 0x82A2A258u, 0);   // 0.0f
    __imp__sub_824EEBA0(ctx, base);
}

// --- the field of view while aiming down the sights (a PC addition) ---------------------------------
//
// CG_CalcFov, sub_8243A198, returns the view's field of view. The hip value
// is the title's cg_fov; while aiming it moves toward the weapon's own
// adsZoomFov (a float at 1600 in the weapon, which the weapon file sets, so
// cg_fov never reaches it) as far as the aim has come. The settings' "field
// of view while aiming" is a per cent of that value, written into the weapon
// in memory: the firing weapon's, found as the function itself finds it
// (the weapon table at 0x82A99E2C, one entry per 5840 bytes of the client's
// state, the client's number at 0x82A2A1C8). A scope's zoom (below
// ScopeBelow degrees) is left as it is, since its picture is a drawn overlay
// that a wider view would not fit, and the result never goes past the hip
// field of view. What was in the weapon is kept, and is put back at 100.
namespace
{
    struct AdsFov { float original; uint32_t written; };

    float AsFloat(uint32_t bits) { float value; memcpy(&value, &bits, sizeof(value)); return value; }
    uint32_t AsBits(float value) { uint32_t bits; memcpy(&bits, &value, sizeof(bits)); return bits; }
}

extern "C" PPC_FUNC(__imp__sub_8243A198);
PPC_FUNC(sub_8243A198)
{
    constexpr uint32_t ClientNumber = 0x82A2A1C8, WeaponTable = 0x82A99E2C, ClientSize = 5840, AdsZoomFov = 1600;
    constexpr float ScopeBelow = 30.0f;
    static std::unordered_map<uint32_t, AdsFov> kept;

    const uint32_t client = Guest::Read32(base, ClientNumber);
    if (client < 4)
    {
        const uint32_t weapon = Guest::Read32(base, WeaponTable + client * ClientSize);
        if (weapon >= 0x10000 && weapon < 0xFFFF0000u)
        {
            const Settings::Values settings = Settings::Get();
            const uint32_t now = Guest::Read32(base, weapon + AdsZoomFov);
            AdsFov& entry = kept[weapon];
            // Not what was written last: the weapon is a fresh load, or never seen.
            if (entry.written == 0 || entry.written != now) entry = { AsFloat(now), 0 };
            float target = entry.original;
            if (settings.adsFov > 100 && entry.original >= ScopeBelow)
                target = std::min(entry.original * float(settings.adsFov) / 100.0f, std::max(entry.original, float(settings.fov)));
            const uint32_t bits = AsBits(target);
            if (bits != now)
            {
                Guest::Write32(base, weapon + AdsZoomFov, bits);
                printf("title: weapon %08X aims at %.1f degrees (the file's %.1f)\n", weapon, target, entry.original);
                fflush(stdout);
            }
            entry.written = bits;
        }
    }
    __imp__sub_8243A198(ctx, base);
}
