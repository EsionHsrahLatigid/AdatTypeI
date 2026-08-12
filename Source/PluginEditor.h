#pragma once

#include <JuceHeader.h>
#include <ehl/juce_design/EhlDesign.h>

class AdatTypeI_VST3AudioProcessor;

class AdatTypeIAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit AdatTypeIAudioProcessorEditor (AdatTypeI_VST3AudioProcessor&);
    ~AdatTypeIAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int defaultWidth = ehl::juce_design::Metrics::defaultWidth;
    static constexpr int defaultHeight = ehl::juce_design::Metrics::defaultHeight;
    static constexpr int minimumWidth = ehl::juce_design::Metrics::minimumWidth;
    static constexpr int minimumHeight = ehl::juce_design::Metrics::minimumHeight;

private:
    ehl::juce_design::LookAndFeel lookAndFeel;
    juce::Viewport controlsViewport;
    juce::GenericAudioProcessorEditor generic;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdatTypeIAudioProcessorEditor)
};
