#include "PluginProcessor.h"

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

void fillSine (juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, std::sin (static_cast<float> (sample) * 0.01f));
}

void requireFinite (const juce::AudioBuffer<float>& buffer)
{
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            require (std::isfinite (buffer.getSample (channel, sample)), "processor emitted non-finite output");
}
} // namespace

int main()
{
    AdatTypeI_VST3AudioProcessor processor;
    processor.prepareToPlay (44100.0, 1024);

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    fillSine (buffer);
    processor.processBlock (buffer, midi);
    requireFinite (buffer);

    juce::AudioBuffer<float> smallBlock (2, 16);
    fillSine (smallBlock);
    processor.processBlock (smallBlock, midi);
    requireFinite (smallBlock);

    juce::AudioBuffer<float> largeBlock (2, 1024);
    largeBlock.clear();
    for (int channel = 0; channel < largeBlock.getNumChannels(); ++channel)
        for (int sample = 0; sample < largeBlock.getNumSamples(); ++sample)
            largeBlock.setSample (channel, sample, 0.25f);

    processor.processBlock (largeBlock, midi);
    requireFinite (largeBlock);

    int nonZeroSamples = 0;
    for (int sample = largeBlock.getNumSamples() / 2; sample < largeBlock.getNumSamples(); ++sample)
        if (std::abs (largeBlock.getSample (0, sample)) > 1.0e-5f
            || std::abs (largeBlock.getSample (1, sample)) > 1.0e-5f)
            ++nonZeroSamples;

    require (nonZeroSamples > largeBlock.getNumSamples() / 4,
             "large block output was truncated after processing a smaller block");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.releaseResources();
    return 0;
}
