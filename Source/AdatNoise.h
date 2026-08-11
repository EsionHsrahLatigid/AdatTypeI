#pragma once
#include "AdatCodec.h"
#include <cstdint>
#include <cmath>
#include <algorithm>

struct XorShift32
{
    uint32_t s = 0x12345678u;

    inline uint32_t next() noexcept
    {
        uint32_t x = s;
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        s = x;
        return x;
    }

    inline float nextFloat() noexcept
    {
        return (next() >> 8) * (1.0f / 16777216.0f); // [0,1)
    }

    inline int nextInt (int maxExclusive) noexcept
    {
        return (int)(next() % (uint32_t)maxExclusive);
    }
};

// small-lambda Poisson (Knuth)
static inline int poissonKnuth (XorShift32& rng, float lambda) noexcept
{
    if (lambda <= 0.0f) return 0;
    const float L = std::exp (-lambda);
    int k = 0;
    float p = 1.0f;
    do { ++k; p *= rng.nextFloat(); } while (p > L);
    return k - 1;
}

struct AdatNoise
{
    float bitFlipRate   = 0.0f; // 0..1 (internally: lambda=256*rate)
    float burstRate     = 0.0f; // 0..1
    int   burstLength   = 32;   // 1..256
    float frameDropRate = 0.0f; // 0..1

    AdatCodec::Frame256 prevFrame{};
    XorShift32 rng{};

    void reset (uint32_t seed = 0xCAFEBABEu) noexcept
    {
        rng.s = seed;
        prevFrame.fill(0);
    }

    inline void apply (AdatCodec::Frame256& frame) noexcept
    {
        // Frame drop: hold previous frame
        if (rng.nextFloat() < frameDropRate)
        {
            frame = prevFrame;
            return;
        }

        // Bit flips: sample k positions instead of scanning 256 bits
        const float lambda = 256.0f * bitFlipRate;
        const int k = poissonKnuth (rng, lambda);

        for (int i = 0; i < k; ++i)
        {
            const int bit = rng.nextInt (256);
            const int w = bit >> 6;
            const int b = bit & 63;
            frame[(size_t)w] ^= (uint64_t)1 << b;
        }

        // Burst: contiguous bit inversions
        if (rng.nextFloat() < burstRate)
        {
            int start = rng.nextInt (256);
            const int len = std::clamp (burstLength, 1, 256);

            for (int i = 0; i < len; ++i)
            {
                const int bit = (start + i) & 255;
                const int w = bit >> 6;
                const int b = bit & 63;
                frame[(size_t)w] ^= (uint64_t)1 << b;
            }
        }

        prevFrame = frame;
    }
};

