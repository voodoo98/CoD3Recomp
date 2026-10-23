#include "../CoD3Host/audio_limiter.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

static void Check(bool pass, const char* message)
{
    if (!pass) throw std::runtime_error(message);
}

int main()
{
    try
    {
        Audio::FrameLimiter limiter;
        std::array<float, 256 * 6> mix{};
        std::array<int16_t, 256 * 6> pcm{};
        mix.fill(0.25f);
        auto result = limiter.Process(mix.data(), pcm.data(), 256, 6, 1.0f, true);
        Check(!result.limited && pcm[0] == int16_t(0.25f * 0.8f * 32767), "Quiet mix changed");
        for (size_t i = 0; i < 256; ++i)
            for (size_t c = 0; c < 6; ++c) mix[i * 6 + c] = c % 2 ? -4.0f : 2.0f;
        result = limiter.Process(mix.data(), pcm.data(), 256, 6, 1.0f, true);
        Check(result.limited && result.preLimitPeak > 32767, "Overload not detected");
        Check(result.peak <= 31784 && result.peak >= 31782, "Limiter ceiling wrong");
        Check(std::abs(pcm[0] * 2 + pcm[1]) <= 1, "Speaker balance changed");
        mix.fill(0.25f);
        result = limiter.Process(mix.data(), pcm.data(), 256, 6, 1.0f, true);
        Check(pcm[0] > 0 && pcm[255 * 6] > pcm[0], "Release did not ramp smoothly");
        for (int i = 0; i < 400; ++i) limiter.Process(mix.data(), pcm.data(), 256, 6, 1.0f, true);
        Check(std::abs(int(pcm[0]) - int(0.25f * 0.8f * 32767)) <= 1, "Gain did not recover");
        limiter.Reset();
        result = limiter.Process(mix.data(), pcm.data(), 256, 2, 0.25f, true);
        Check(pcm[0] == int16_t(0.25f * 0.8f * 0.25f * 32767), "Output volume ignored");
        mix.fill(4.0f);
        limiter.Reset();
        result = limiter.Process(mix.data(), pcm.data(), 256, 2, 1.0f, false);
        Check(pcm[0] == 32767 && !result.limited, "Disabled limiter did not preserve clipping");
        result = limiter.Process(mix.data(), pcm.data(), 256, 2, 0.0f, true);
        Check(result.peak == 0 && pcm[0] == 0, "Mute is not silent");
        mix.fill(0.0f);
        mix[0] = std::numeric_limits<float>::quiet_NaN();
        mix[1] = std::numeric_limits<float>::infinity();
        mix[2] = -std::numeric_limits<float>::infinity();
        result = limiter.Process(mix.data(), pcm.data(), 256, 6, 1.0f, true);
        Check(result.peak == 0 && pcm[0] == 0 && pcm[1] == 0 && pcm[2] == 0, "Non-finite sample made noise");
        std::cout << "PASS: ceiling, channel balance, recovery, stereo volume, mute, bypass and invalid samples\n";
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
