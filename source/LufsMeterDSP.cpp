#include "LufsMeterDSP.h"
#include <cmath>
#include <numbers>

namespace TrueLevel
{

LufsMeterDSP::LufsMeterDSP()
{
    gatedBlocks.reserve(MaxGatedBlocks);
}

void LufsMeterDSP::prepare(double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = (sampleRate > 8000.0) ? sampleRate : 48000.0;
    calculateFilterCoefficients(currentSampleRate);

    momentarySize     = static_cast<size_t>(currentSampleRate * 0.4);
    shortTermSize     = static_cast<size_t>(currentSampleRate * 3.0);
    blockSize100ms    = static_cast<size_t>(currentSampleRate * 0.1);
    corrWindowSamples = blockSize100ms;

    momentaryPowerL.assign(momentarySize, 0.0);
    momentaryPowerR.assign(momentarySize, 0.0);
    shortTermPowerL.assign(shortTermSize, 0.0);
    shortTermPowerR.assign(shortTermSize, 0.0);

    reset();
}

void LufsMeterDSP::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        shelfState[ch] = {};
        rlbState[ch]   = {};
    }

    std::fill(momentaryPowerL.begin(), momentaryPowerL.end(), 0.0);
    std::fill(momentaryPowerR.begin(), momentaryPowerR.end(), 0.0);
    momentaryIndex = 0;
    momentaryRunningSumL = 0.0;
    momentaryRunningSumR = 0.0;

    std::fill(shortTermPowerL.begin(), shortTermPowerL.end(), 0.0);
    std::fill(shortTermPowerR.begin(), shortTermPowerR.end(), 0.0);
    shortTermIndex = 0;
    shortTermRunningSumL = 0.0;
    shortTermRunningSumR = 0.0;

    resetIntegrated();

    corrSumLR = 0.0;
    corrSumLL = 0.0;
    corrSumRR = 0.0;
    corrCount = 0;

    momentaryLufs.store(-100.0f, std::memory_order_relaxed);
    shortTermLufs.store(-100.0f, std::memory_order_relaxed);
    stereoCorrelation.store(1.0f, std::memory_order_relaxed);
    peakLeft.store(-100.0f, std::memory_order_relaxed);
    peakRight.store(-100.0f, std::memory_order_relaxed);
}

void LufsMeterDSP::resetIntegrated() noexcept
{
    pendingIntegratedReset.store(true, std::memory_order_release);
}

void LufsMeterDSP::calculateFilterCoefficients(double sampleRate)
{
    const double dbGain = 3.999;
    const double f0_1   = 1681.9744509555319;
    const double Q1     = 0.7071752369554193;
    const double K1     = std::tan(std::numbers::pi * f0_1 / sampleRate);
    const double Vh     = std::pow(10.0, dbGain / 20.0);
    const double Vb     = std::pow(Vh, 0.499666774155);

    const double a0_1 = 1.0 + K1 / Q1 + K1 * K1;
    highShelfCoeffs.b0 = (Vh + Vb * (K1 / Q1) + K1 * K1) / a0_1;
    highShelfCoeffs.b1 = (2.0 * (K1 * K1 - Vh)) / a0_1;
    highShelfCoeffs.b2 = (Vh - Vb * (K1 / Q1) + K1 * K1) / a0_1;
    highShelfCoeffs.a1 = (2.0 * (K1 * K1 - 1.0)) / a0_1;
    highShelfCoeffs.a2 = (1.0 - K1 / Q1 + K1 * K1) / a0_1;

    const double f0_2 = 38.13547087602444;
    const double Q2   = 0.5003270373253953;
    const double K2   = std::tan(std::numbers::pi * f0_2 / sampleRate);

    const double a0_2 = 1.0 + K2 / Q2 + K2 * K2;
    rlbHighPassCoeffs.b0 = 1.0 / a0_2;
    rlbHighPassCoeffs.b1 = -2.0 / a0_2;
    rlbHighPassCoeffs.b2 = 1.0 / a0_2;
    rlbHighPassCoeffs.a1 = (2.0 * (K2 * K2 - 1.0)) / a0_2;
    rlbHighPassCoeffs.a2 = (1.0 - K2 / Q2 + K2 * K2) / a0_2;
}

float LufsMeterDSP::powerToLufs(double power) noexcept
{
    if (power <= 1e-12)
        return -100.0f;
    return static_cast<float>(-0.691 + 10.0 * std::log10(power));
}

void LufsMeterDSP::process(const float* const* channelData, int numChannels, int numSamples)
{
    if (numChannels < 2 || numSamples <= 0)
        return;

    if (pendingIntegratedReset.exchange(false, std::memory_order_acq_rel))
    {
        gatedBlocks.clear();
        blockCounter = 0;
        currentBlockSumL = 0.0;
        currentBlockSumR = 0.0;
        integratedLufs.store(-100.0f, std::memory_order_relaxed);
    }

    const float* inL = channelData[0];
    const float* inR = channelData[1];

    float maxSampleL = 0.0f;
    float maxSampleR = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        const float sampleL = inL[i];
        const float sampleR = inR[i];

        maxSampleL = std::max(maxSampleL, std::abs(sampleL));
        maxSampleR = std::max(maxSampleR, std::abs(sampleR));

        // Transposed Direct-Form II Filtering
        const double xL  = static_cast<double>(sampleL);
        const double y1L = highShelfCoeffs.b0 * xL + shelfState[0].s1;
        shelfState[0].s1 = highShelfCoeffs.b1 * xL - highShelfCoeffs.a1 * y1L + shelfState[0].s2;
        shelfState[0].s2 = highShelfCoeffs.b2 * xL - highShelfCoeffs.a2 * y1L;

        const double y2L = rlbHighPassCoeffs.b0 * y1L + rlbState[0].s1;
        rlbState[0].s1   = rlbHighPassCoeffs.b1 * y1L - rlbHighPassCoeffs.a1 * y2L + rlbState[0].s2;
        rlbState[0].s2   = rlbHighPassCoeffs.b2 * y1L - rlbHighPassCoeffs.a2 * y2L;

        const double xR  = static_cast<double>(sampleR);
        const double y1R = highShelfCoeffs.b0 * xR + shelfState[1].s1;
        shelfState[1].s1 = highShelfCoeffs.b1 * xR - highShelfCoeffs.a1 * y1R + shelfState[1].s2;
        shelfState[1].s2 = highShelfCoeffs.b2 * xR - highShelfCoeffs.a2 * y1R;

        const double y2R = rlbHighPassCoeffs.b0 * y1R + rlbState[1].s1;
        rlbState[1].s1   = rlbHighPassCoeffs.b1 * y1R - rlbHighPassCoeffs.a1 * y2R + rlbState[1].s2;
        rlbState[1].s2   = rlbHighPassCoeffs.b2 * y1R - rlbHighPassCoeffs.a2 * y2R;

        const double pL = y2L * y2L;
        const double pR = y2R * y2R;

        momentaryRunningSumL += pL - momentaryPowerL[momentaryIndex];
        momentaryRunningSumR += pR - momentaryPowerR[momentaryIndex];
        momentaryPowerL[momentaryIndex] = pL;
        momentaryPowerR[momentaryIndex] = pR;
        if (++momentaryIndex >= momentarySize)
            momentaryIndex = 0;

        shortTermRunningSumL += pL - shortTermPowerL[shortTermIndex];
        shortTermRunningSumR += pR - shortTermPowerR[shortTermIndex];
        shortTermPowerL[shortTermIndex] = pL;
        shortTermPowerR[shortTermIndex] = pR;
        if (++shortTermIndex >= shortTermSize)
            shortTermIndex = 0;

        currentBlockSumL += pL;
        currentBlockSumR += pR;
        if (++blockCounter >= blockSize100ms)
        {
            const double blkL = currentBlockSumL / static_cast<double>(blockSize100ms);
            const double blkR = currentBlockSumR / static_cast<double>(blockSize100ms);
            const float blkLufs = powerToLufs(blkL + blkR);

            if (blkLufs > -70.0f && gatedBlocks.size() < MaxGatedBlocks)
            {
                gatedBlocks.push_back({ blkL, blkR, blkLufs });
            }

            blockCounter = 0;
            currentBlockSumL = 0.0;
            currentBlockSumR = 0.0;
        }

        corrSumLR += static_cast<double>(sampleL * sampleR);
        corrSumLL += static_cast<double>(sampleL * sampleL);
        corrSumRR += static_cast<double>(sampleR * sampleR);
        if (++corrCount >= corrWindowSamples)
        {
            const double denom = std::sqrt(corrSumLL * corrSumRR);
            float corr = 1.0f;
            if (denom > 1e-9)
            {
                corr = static_cast<float>(corrSumLR / denom);
                corr = std::clamp(corr, -1.0f, 1.0f);
            }
            stereoCorrelation.store(corr, std::memory_order_relaxed);

            corrSumLR = 0.0;
            corrSumLL = 0.0;
            corrSumRR = 0.0;
            corrCount = 0;
        }
    }

    const double momMean = (std::max(0.0, momentaryRunningSumL) + std::max(0.0, momentaryRunningSumR)) / static_cast<double>(momentarySize);
    momentaryLufs.store(powerToLufs(momMean), std::memory_order_relaxed);

    const double stMean = (std::max(0.0, shortTermRunningSumL) + std::max(0.0, shortTermRunningSumR)) / static_cast<double>(shortTermSize);
    shortTermLufs.store(powerToLufs(stMean), std::memory_order_relaxed);

    if (!gatedBlocks.empty())
    {
        double sumUngatedL = 0.0;
        double sumUngatedR = 0.0;
        for (const auto& blk : gatedBlocks)
        {
            sumUngatedL += blk.powerL;
            sumUngatedR += blk.powerR;
        }
        const double meanUngatedL = sumUngatedL / static_cast<double>(gatedBlocks.size());
        const double meanUngatedR = sumUngatedR / static_cast<double>(gatedBlocks.size());
        const float ungatedLufs = powerToLufs(meanUngatedL + meanUngatedR);

        const float relativeThreshold = ungatedLufs - 10.0f;

        double gatedSumL = 0.0;
        double gatedSumR = 0.0;
        size_t passCount = 0;
        for (const auto& blk : gatedBlocks)
        {
            if (blk.lufs > relativeThreshold)
            {
                gatedSumL += blk.powerL;
                gatedSumR += blk.powerR;
                passCount++;
            }
        }

        if (passCount > 0)
        {
            const double finalMeanL = gatedSumL / static_cast<double>(passCount);
            const double finalMeanR = gatedSumR / static_cast<double>(passCount);
            integratedLufs.store(powerToLufs(finalMeanL + finalMeanR), std::memory_order_relaxed);
        }
    }

    const float pkL = maxSampleL > 1e-5f ? 20.0f * std::log10(maxSampleL) : -100.0f;
    const float pkR = maxSampleR > 1e-5f ? 20.0f * std::log10(maxSampleR) : -100.0f;
    peakLeft.store(pkL, std::memory_order_relaxed);
    peakRight.store(pkR, std::memory_order_relaxed);
}

} // namespace TrueLevel