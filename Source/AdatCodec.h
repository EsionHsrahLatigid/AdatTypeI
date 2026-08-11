#pragma once
#include <array>
#include <cstdint>
#include <cmath>
#include <algorithm>

struct AdatCodec
{
    using Frame256 = std::array<uint64_t, 4>;

    static inline int16_t floatToI16(float x) noexcept
    {
        // A/D相当：0 dBFS でハードクリップ
        x = std::clamp(x, -1.0f, 1.0f);

        // 1.0f は +32767 に飽和、-1.0f は -32768 に飽和させる
        if (x >= 1.0f)  return (int16_t) 32767;
        if (x <= -1.0f) return (int16_t) -32768;

        // 通常域
        const float s = x * 32768.0f;             // -1.0 -> -32768 近傍
        int v = (int) std::lrintf(s);
        v = std::clamp(v, -32768, 32767);
        return (int16_t) v;
    }

    static inline float i16ToFloat(int16_t v) noexcept
    {
        return (float) v / 32768.0f;
    }

    static inline void setBit(Frame256& f, int bitIndex, uint32_t bit) noexcept
    {
        const int w = bitIndex >> 6;
        const int b = bitIndex & 63;
        const uint64_t mask = (uint64_t)1 << b;
        if (bit) f[(size_t)w] |= mask;
        else     f[(size_t)w] &= ~mask;
    }

    static inline uint32_t getBit(const Frame256& f, int bitIndex) noexcept
    {
        const int w = bitIndex >> 6;
        const int b = bitIndex & 63;
        return (uint32_t)((f[(size_t)w] >> b) & 1u);
    }

    // 8ch float sample -> NRZI frame
    static inline Frame256 encodeSample(const float* inCh8) noexcept
    {
        std::array<uint32_t, 8> w24{};
        for (int ch = 0; ch < 8; ++ch)
        {
            const int16_t q = floatToI16(inCh8[ch]);
            const int32_t s24 = ((int32_t)q) << 8; // lower 8 bits = 0
            w24[(size_t)ch] = static_cast<uint32_t>(s24) & 0x00FFFFFFu;
        }

        // Pack 192 bits -> 48 nibbles (MSB->LSB)
        std::array<uint8_t, 48> nibbles{};
        int nibIdx = 0;
        uint8_t cur = 0;
        int curBits = 0;

        auto pushBit = [&](uint32_t bit) noexcept
        {
            cur = static_cast<uint8_t>((static_cast<uint32_t>(cur) << 1u) | (bit & 1u));
            if (++curBits == 4)
            {
                nibbles[(size_t)nibIdx++] = cur;
                cur = 0; curBits = 0;
            }
        };

        for (int ch = 0; ch < 8; ++ch)
            for (int b = 23; b >= 0; --b)
                pushBit((w24[(size_t)ch] >> b) & 1u);

        Frame256 data{};
        data.fill(0);

        int outBit = 0;
        // 240 bits: nibble 4bits + stuffed '1'
        for (int i = 0; i < 48; ++i)
        {
            const uint8_t n = nibbles[(size_t)i];
            for (int b = 3; b >= 0; --b)
                setBit(data, outBit++, (n >> b) & 1u);
            setBit(data, outBit++, 1u);
        }

        // 16 bits: sync/user (simple, deterministic)
        for (int i = 0; i < 10; ++i) setBit(data, outBit++, 0u);
        setBit(data, outBit++, 1u);
        for (int i = 0; i < 4;  ++i) setBit(data, outBit++, 0u);
        setBit(data, outBit++, 1u);

        // NRZI encode
        Frame256 nrzi{};
        nrzi.fill(0);
        uint32_t level = 1u;

        for (int i = 0; i < 256; ++i)
        {
            const uint32_t bit = getBit(data, i);
            if (bit == 1u) level ^= 1u;
            setBit(nrzi, i, level);
        }

        return nrzi;
    }

    // NRZI frame -> 8ch float sample
    static inline void decodeSample(const Frame256& nrzi, float* outCh8) noexcept
    {
        Frame256 data{};
        data.fill(0);

        uint32_t prevLevel = 1u;
        for (int i = 0; i < 256; ++i)
        {
            const uint32_t level = getBit(nrzi, i);
            const uint32_t bit = (level ^ prevLevel) & 1u;
            setBit(data, i, bit);
            prevLevel = level;
        }

        std::array<uint8_t, 48> nibbles{};
        int bitPos = 0;
        for (int i = 0; i < 48; ++i)
        {
            uint8_t n = 0;
            for (int b = 0; b < 4; ++b)
                n = (uint8_t)((n << 1) | (uint8_t)getBit(data, bitPos++));
            (void)getBit(data, bitPos++); // stuffed bit
            nibbles[(size_t)i] = n;
        }

        std::array<uint32_t, 8> w24{};
        int nib = 0;
        int bitInGroup = 0;

        auto pullBit = [&]() noexcept -> uint32_t
        {
            const uint8_t n = nibbles[(size_t)nib];
            const int idx = 3 - (bitInGroup & 3);
            const uint32_t bit = (n >> idx) & 1u;
            if (++bitInGroup == 4) { bitInGroup = 0; ++nib; }
            return bit;
        };

        for (int ch = 0; ch < 8; ++ch)
        {
            uint32_t w = 0;
            for (int b = 0; b < 24; ++b)
                w = (w << 1) | pullBit();
            w24[(size_t)ch] = w & 0x00FFFFFFu;
        }

        for (int ch = 0; ch < 8; ++ch)
        {
            int32_t s = (int32_t)w24[(size_t)ch];
            if (s & 0x00800000) s |= (int32_t)0xFF000000; // sign-extend 24->32
            const int16_t q = (int16_t)(s >> 8);
            outCh8[ch] = i16ToFloat(q);
        }
    }
};
