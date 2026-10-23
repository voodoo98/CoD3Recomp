#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Audio
{
    // Linked-channel limiter adapted from legoliamneeson/CoD3Recomp fa156773e6.
    // One call is one 256-sample audio frame at 48 kHz. All speakers share the
    // same gain so limiting never changes a sound's direction.
    class FrameLimiter
    {
        float gain = 1.0f;

    public:
        struct Result { int peak, preLimitPeak; bool limited; };
        void Reset() { gain = 1.0f; }

        Result Process(float* mixed, int16_t* output, size_t frames, uint32_t channels,
                       float volume, bool enabled)
        {
            if (frames == 0 || channels == 0) return {};
            const float scale = 0.8f * (std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 0.0f);
            float peak = 0.0f;
            for (size_t i = 0; i < frames * channels; ++i)
            {
                if (!std::isfinite(mixed[i])) mixed[i] = 0.0f;
                peak = std::max(peak, std::fabs(mixed[i]));
            }
            constexpr float Ceiling = 0.97f;
            constexpr float Release = 0.026f; // about 200 ms recovery
            const float loudest = peak * scale;
            const float before = gain;
            float after = before + (1.0f - before) * Release;
            if (enabled && loudest * after > Ceiling) after = Ceiling / loudest;
            if (!enabled) after = 1.0f;
            gain = after;
            Result result{0, int(std::min(loudest, 100.0f) * 32767.0f), after < before};
            for (size_t i = 0; i < frames; ++i)
            {
                const float ramp = after < before ? after : before + (after - before) * (float(i + 1) / float(frames));
                for (uint32_t c = 0; c < channels; ++c)
                {
                    const size_t at = i * channels + c;
                    const float pcm = mixed[at] * ramp * scale * 32767.0f;
                    output[at] = int16_t(std::clamp(pcm, -32768.0f, 32767.0f));
                    result.peak = std::max(result.peak, std::abs(int(output[at])));
                }
            }
            return result;
        }
    };
}
