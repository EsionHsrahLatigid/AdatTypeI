#include "PluginProcessor.h"

// JUCE plugin clients (AU/VST3) が参照するエントリポイント
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AdatTypeI_VST3AudioProcessor();
}

