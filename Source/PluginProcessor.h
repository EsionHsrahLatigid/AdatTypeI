#pragma once
#include <JuceHeader.h>
#include "AdatCodec.h"
#include "StreamingResampler.h"
#include "AdatNoise.h"

class AdatTypeI_VST3AudioProcessor final : public juce::AudioProcessor
{
public:
    AdatTypeI_VST3AudioProcessor();
    ~AdatTypeI_VST3AudioProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    //==============================================================================
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    //==============================================================================
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    //==============================================================================
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

private:
    void processAdat48k (const juce::AudioBuffer<float>& in48,
                         juce::AudioBuffer<float>& out48,
                         int n48);

    StreamingResampler to48k, from48k;

    juce::AudioBuffer<float> inPadded8;
    juce::AudioBuffer<float> work48;
    juce::AudioBuffer<float> out48;
    juce::AudioBuffer<float> outHost8;

    AdatNoise noise;
    double to48Remainder;
    int maxHostBlockSamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdatTypeI_VST3AudioProcessor)
};
