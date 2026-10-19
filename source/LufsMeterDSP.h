#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <vector>
#include <cmath>

namespace TrueLevel
{

class LufsMeterDSP
{
public:
    LufsMeterDSP();
    ~LufsMeterDSP() = default;

    void prepare(double sampleRate, int maxBlockSize);
    void reset();
    void resetIntegrated() noexcept;

    void process(const float* const* channelData, int numChannels, int numSamples);

    float getMomentaryLufs() const noexcept    { return momentaryLufs.load(std::memory_order_relaxed); }
    float getShortTermLufs() const noexcept    { return shortTermLufs.load(std::memory_order_relaxed); }
    float getIntegratedLufs() const noexcept   { return integratedLufs.load(std::memory_order_relaxed); }
    float getStereoCorrelation() const noexcept{ return stereoCorrelation.load(std::memory_order_relaxed); }
    float getPeakLeft() const noexcept         { return peakLeft.load(std::memory_order_relaxed); }
    float getPeakRight() const noexcept        { return peakRight.load(std::memory_order_relaxed); }

private:
    void calculateFilterCoefficients(double sampleRate);
    static float powerToLufs(double power) noexcept;

    double currentSampleRate { 48000.0 };

    struct FilterCoefficients
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 };
        double a1 { 0.0 }, a2 { 0.0 };
    };

    struct FilterState
    {
        double s1 { 0.0 };
        double s2 { 0.0 };
    };

    FilterCoefficients highShelfCoeffs;
    FilterCoefficients rlbHighPassCoeffs;

    FilterState shelfState[2];
    FilterState rlbState[2];

    std::vector<double> momentaryPowerL;
    std::vector<double> momentaryPowerR;
    size_t momentaryIndex { 0 };
    size_t momentarySize { 19200 };
    double momentaryRunningSumL { 0.0 };
    double momentaryRunningSumR { 0.0 };

    std::vector<double> shortTermPowerL;
    std::vector<double> shortTermPowerR;
    size_t shortTermIndex { 0 };
    size_t shortTermSize { 144000 };
    double shortTermRunningSumL { 0.0 };
    double shortTermRunningSumR { 0.0 };

    struct GatedBlock
    {
        double powerL;
        double powerR;
        float lufs;
    };
    static constexpr size_t MaxGatedBlocks = 36000;
    std::vector<GatedBlock> gatedBlocks;
    size_t blockCounter { 0 };
    size_t blockSize100ms { 4800 };
    double currentBlockSumL { 0.0 };
    double currentBlockSumR { 0.0 };

    double corrSumLR { 0.0 };
    double corrSumLL { 0.0 };
    double corrSumRR { 0.0 };
    size_t corrCount { 0 };
    size_t corrWindowSamples { 4800 };

    std::atomic<float> momentaryLufs { -100.0f };
    std::atomic<float> shortTermLufs { -100.0f };
    std::atomic<float> integratedLufs { -100.0f };
    std::atomic<float> stereoCorrelation { 1.0f };
    std::atomic<float> peakLeft { -100.0f };
    std::atomic<float> peakRight { -100.0f };
    std::atomic<bool>  pendingIntegratedReset { false };
};

} // namespace TrueLevel