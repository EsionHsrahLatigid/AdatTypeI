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
} // namespace

int main()
{
    AdatTypeI_VST3AudioProcessor processor;
    processor.prepareToPlay (44100.0, 128);

    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            buffer.setSample (channel, sample, std::sin (static_cast<float> (sample) * 0.01f));

    processor.processBlock (buffer, midi);

    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
            require (std::isfinite (buffer.getSample (channel, sample)), "processor emitted non-finite output");

    juce::MemoryBlock state;
    processor.getStateInformation (state);
    processor.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
    processor.releaseResources();
    return 0;
}
