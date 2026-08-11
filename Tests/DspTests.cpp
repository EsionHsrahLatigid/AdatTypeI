#include "AdatCodec.h"
#include "AdatNoise.h"

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>

namespace
{
void require (bool condition, const char* message)
{
    if (! condition)
    {
        std::cerr << message << '\n';
        std::exit (1);
    }
}

void codecRoundTripIsBounded()
{
    std::array<float, 8> input { -1.0f, -0.75f, -0.25f, 0.0f, 0.25f, 0.5f, 0.75f, 0.999f };
    std::array<float, 8> output {};

    const auto frame = AdatCodec::encodeSample (input.data());
    AdatCodec::decodeSample (frame, output.data());

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        require (std::isfinite (output[i]), "decoded sample is not finite");
        require (std::abs (input[i] - output[i]) < 1.0f / 32768.0f, "codec round trip drifted beyond one LSB");
    }
}

void noiseIsDeterministicForSameSeed()
{
    std::array<float, 8> input {};
    input[0] = 0.5f;

    auto a = AdatCodec::encodeSample (input.data());
    auto b = a;

    AdatNoise noiseA;
    AdatNoise noiseB;
    noiseA.bitFlipRate = noiseB.bitFlipRate = 0.01f;
    noiseA.burstRate = noiseB.burstRate = 0.25f;
    noiseA.burstLength = noiseB.burstLength = 12;
    noiseA.frameDropRate = noiseB.frameDropRate = 0.05f;
    noiseA.reset (0x1234u);
    noiseB.reset (0x1234u);

    for (int i = 0; i < 64; ++i)
    {
        noiseA.apply (a);
        noiseB.apply (b);
        require (a == b, "ADAT noise must be deterministic for identical seeds");
    }
}
} // namespace

int main()
{
    codecRoundTripIsBounded();
    noiseIsDeterministicForSameSeed();
    return 0;
}
