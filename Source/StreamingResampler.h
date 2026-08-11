#pragma once
#include <JuceHeader.h>

class StreamingResampler
{
public:
    void prepare (int numChannels, double inSr, double outSr, int maxBlockSize)
    {
        jassert (numChannels >= 1 && numChannels <= 8);
        ch = numChannels;

        inputSr = inSr;
        outputSr = outSr;
        ratio = inputSr / outputSr; // input advance per output sample

        const int fifoCap = maxBlockSize * 4 + 128;
        fifo.setSize (ch, fifoCap, false, true, true);
        fifo.clear();

        fifoSize = fifoCap;
        fifoWrite = fifoRead = 0;
        available = 0;

        interpolators.clear();
        interpolators.resize ((size_t) ch);
        for (auto& it : interpolators) it.reset();

        tempIn.resize ((size_t) fifoCap);
    }

    void reset()
    {
        for (auto& it : interpolators) it.reset();
        fifo.clear();
        fifoWrite = fifoRead = 0;
        available = 0;
    }

    void pushInput (const juce::AudioBuffer<float>& in, int numSamples)
    {
        jassert (in.getNumChannels() >= ch);
        ensureSpace (numSamples);

        for (int c = 0; c < ch; ++c)
        {
            const float* src = in.getReadPointer (c);
            int n = numSamples;
            int w = fifoWrite;

            while (n > 0)
            {
                const int chunk = std::min (n, fifoSize - w);
                fifo.copyFrom (c, w, src + (numSamples - n), chunk);
                n -= chunk;
                w = (w + chunk) % fifoSize;
            }
        }

        fifoWrite = (fifoWrite + numSamples) % fifoSize;
        available += numSamples;
    }

    // outSamples を生成。入力不足なら残りを0埋め。
    void popOutput (juce::AudioBuffer<float>& out, int outSamples)
    {
        jassert (out.getNumChannels() >= ch);

        const int minInGuard = 16;

        // 1回のpopOutputで、全チャンネルが同じ入力位置を使う
        int produced = 0;

        while (produced < outSamples)
        {
            if (available < minInGuard)
            {
                for (int c = 0; c < ch; ++c)
                {
                    float* dst = out.getWritePointer (c);
                    std::fill (dst + produced, dst + outSamples, 0.0f);
                }
                break;
            }

            const int wantOut = outSamples - produced;
            const int maxInNeeded = std::min ((int) tempIn.size(),
                                              (int) std::ceil (wantOut * ratio) + minInGuard);

            int usedInCommon = -1;

            for (int c = 0; c < ch; ++c)
            {
                // fifoRead から maxInNeeded サンプルを連続抽出（リング→直列化）。
                // ガード領域は有効入力の末尾値で埋め、未投入のFIFO領域を読まない。
                int r = fifoRead;
                float tail = 0.0f;
                for (int i = 0; i < maxInNeeded; ++i)
                {
                    if (i < available)
                    {
                        tail = fifo.getSample (c, r);
                        tempIn[(size_t) i] = tail;
                        r = (r + 1) % fifoSize;
                    }
                    else
                    {
                        tempIn[(size_t) i] = tail;
                    }
                }

                float* dst = out.getWritePointer (c);
                const int usedIn = interpolators[(size_t)c].process (ratio,
                                                                     tempIn.data(),
                                                                     dst + produced,
                                                                     wantOut);

                if (c == 0)
                    usedInCommon = usedIn;
                else
                    jassert (usedIn == usedInCommon); // ここが崩れるならロジック見直し
            }

            // readポインタは“1回だけ”進める
            if (usedInCommon <= 0)
            {
                // 変な状態の保険：残りを無音
                for (int c = 0; c < ch; ++c)
                {
                    float* dst = out.getWritePointer (c);
                    std::fill (dst + produced, dst + outSamples, 0.0f);
                }
                break;
            }

            fifoRead = (fifoRead + usedInCommon) % fifoSize;
            available -= usedInCommon;

            produced = outSamples; // wantOut全部作った前提
        }
    }
private:
    void ensureSpace (int incoming)
    {
        const int free = fifoSize - available - 1;
        if (incoming <= free) return;

        const int drop = incoming - free;
        fifoRead = (fifoRead + drop) % fifoSize;
        available -= drop;
        if (available < 0) available = 0;
    }

    int ch = 0;
    double inputSr = 48000.0, outputSr = 48000.0;
    double ratio = 1.0;

    juce::AudioBuffer<float> fifo;
    int fifoSize = 0;
    int fifoWrite = 0, fifoRead = 0;
    int available = 0;

    std::vector<juce::CatmullRomInterpolator> interpolators;
    std::vector<float> tempIn;
};
