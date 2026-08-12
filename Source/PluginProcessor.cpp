#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace ParamIDs
{
    static constexpr const char* bitFlip   = "bitFlip";
    static constexpr const char* burstRate = "burstRate";
    static constexpr const char* burstLen  = "burstLen";
    static constexpr const char* frameDrop = "frameDrop";
}

AdatTypeI_VST3AudioProcessor::AdatTypeI_VST3AudioProcessor()
: juce::AudioProcessor (
      BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
  ),
  apvts (*this, nullptr, "PARAMS", createParameterLayout())
{
    noise.reset();
}

juce::AudioProcessorValueTreeState::ParameterLayout
AdatTypeI_VST3AudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using API = juce::AudioParameterInt;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    // 安全寄りのレンジ（必要なら上限上げてOK）
    p.push_back (std::make_unique<APF>(ParamIDs::bitFlip,   "Bit Flip Rate",
                                       juce::NormalisableRange<float>(0.0f, 0.02f, 0.00001f, 0.5f), 0.0f));
    p.push_back (std::make_unique<APF>(ParamIDs::burstRate, "Burst Rate",
                                       juce::NormalisableRange<float>(0.0f, 0.01f, 0.00001f, 0.5f), 0.0f));
    p.push_back (std::make_unique<API>(ParamIDs::burstLen,  "Burst Length", 1, 256, 32));
    p.push_back (std::make_unique<APF>(ParamIDs::frameDrop, "Frame Drop Rate",
                                       juce::NormalisableRange<float>(0.0f, 0.001f, 0.000001f, 0.5f), 0.0f));

    return { p.begin(), p.end() };
}

void AdatTypeI_VST3AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const int maxCh = 8;
    const double effectiveSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    preparedSampleRate = effectiveSampleRate;
    maxHostBlockSamples = std::max (1, samplesPerBlock);

    inPadded8.setSize (maxCh, maxHostBlockSamples, false, true, true);
    inPadded8.clear();

    // host -> 48k (ceil + margin)
    const int max48 = (int) std::ceil ((double)maxHostBlockSamples * (48000.0 / effectiveSampleRate)) + 64;
    work48.setSize (maxCh, max48, false, true, true);
    out48.setSize  (maxCh, max48, false, true, true);
    outHost8.setSize (maxCh, maxHostBlockSamples, false, true, true);

    to48k.prepare   (maxCh, effectiveSampleRate, 48000.0, maxHostBlockSamples);
    from48k.prepare (maxCh, 48000.0, effectiveSampleRate, max48);

    to48k.reset();
    from48k.reset();

    noise.reset ((uint32_t) juce::Random::getSystemRandom().nextInt());

    to48Remainder = 0.0;
}

void AdatTypeI_VST3AudioProcessor::releaseResources()
{
}

bool AdatTypeI_VST3AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();

    if (in.isDisabled() || out.isDisabled())
        return false;

    if (in != out)
        return false;

    const int ch = in.size();
    if (ch < 1 || ch > 8)
        return false;

    // discreteChannelsも許可（ホストによってはstereo/mono表現になるため）
    return true;
}

juce::AudioProcessorEditor* AdatTypeI_VST3AudioProcessor::createEditor()
{
    return new AdatTypeIAudioProcessorEditor (*this);
}

void AdatTypeI_VST3AudioProcessor::processAdat48k (const juce::AudioBuffer<float>& in48,
                                                  juce::AudioBuffer<float>& out48b,
                                                  int n48)
{
    float in8[8];
    float out8[8];

    // パラメータ反映（ブロック先頭でOK）
    noise.bitFlipRate   = apvts.getRawParameterValue (ParamIDs::bitFlip)->load();
    noise.burstRate     = apvts.getRawParameterValue (ParamIDs::burstRate)->load();
    noise.burstLength   = (int) apvts.getRawParameterValue (ParamIDs::burstLen)->load();
    noise.frameDropRate = apvts.getRawParameterValue (ParamIDs::frameDrop)->load();

    for (int n = 0; n < n48; ++n)
    {
        for (int c = 0; c < 8; ++c)
            in8[c] = in48.getReadPointer(c)[n];

        auto frame = AdatCodec::encodeSample (in8);
        noise.apply (frame);
        AdatCodec::decodeSample (frame, out8);

        for (int c = 0; c < 8; ++c)
            out48b.getWritePointer(c)[n] = out8[c];
    }
}

void AdatTypeI_VST3AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                                juce::MidiBuffer& midi)
{
    juce::ignoreUnused (midi);
    juce::ScopedNoDenormals noDenormals;

    const int hostCh = buffer.getNumChannels();
    const int hostN  = buffer.getNumSamples();
    const double hostSR = preparedSampleRate;

    if (hostN <= 0)
        return;

    for (int hostOffset = 0; hostOffset < hostN; hostOffset += maxHostBlockSamples)
    {
        const int processHostN = std::min (maxHostBlockSamples, hostN - hostOffset);

        // 1) 入力を8chに整形（不足ch=0埋め）
        inPadded8.clear (0, processHostN);
        for (int c = 0; c < std::min (hostCh, 8); ++c)
            inPadded8.copyFrom (c, 0, buffer, c, hostOffset, processHostN);

        // 2) host -> 48k
        to48k.pushInput (inPadded8, processHostN);

        const double exact48 = (double) processHostN * (48000.0 / hostSR) + to48Remainder;
        const int n48 = (int) std::floor (exact48);
        to48Remainder = exact48 - (double) n48;

        // 念のため最低1、最大は確保バッファ内に丸める
        const int n48Safe = std::clamp (n48, 1, work48.getNumSamples());

        work48.clear (0, n48Safe);
        out48.clear (0, n48Safe);

        to48k.popOutput (work48, n48Safe);

        // 3) 48k領域でADAT loop + noise
        processAdat48k (work48, out48, n48Safe);

        // 4) 48k -> host
        from48k.pushInput (out48, n48Safe);

        outHost8.clear (0, processHostN);
        from48k.popOutput (outHost8, processHostN);

        // 5) 出力（ホストch数分のみ）
        for (int c = 0; c < hostCh; ++c)
        {
            if (c < 8) buffer.copyFrom (c, hostOffset, outHost8, c, 0, processHostN);
            else       buffer.clear (c, hostOffset, processHostN);
        }
    }
}

void AdatTypeI_VST3AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void AdatTypeI_VST3AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState && xmlState->hasTagName (apvts.state.getType()))
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
}
