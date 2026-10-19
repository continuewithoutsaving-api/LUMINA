#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "RingBuffer.h"
#include "LufsMeterDSP.h"

namespace TrueLevel
{

class TrueLevelAudioProcessor : public juce::AudioProcessor,
                                public juce::AudioProcessorValueTreeState::Listener
{
public:
    TrueLevelAudioProcessor();
    ~TrueLevelAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "TrueLevel"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    // Thread-safe accessors for PluginEditor
    LufsMeterDSP& getMeterDSP() noexcept { return meterDSP; }
    LockFreeRingBuffer<GoniometerSample, 4096>& getGoniometerQueue() noexcept { return goniometerQueue; }
    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }

    float getTargetLufs() const noexcept;
    float getCurrentGainMatchOffsetDb() const noexcept { return currentGainMatchOffsetDb.load(std::memory_order_relaxed); }

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

private:
    juce::AudioProcessorValueTreeState apvts;
    LufsMeterDSP meterDSP;
    LockFreeRingBuffer<GoniometerSample, 4096> goniometerQueue;

    // Scratch channel read pointers (pre-allocated)
    std::vector<const float*> channelPointers;

    // Vectorized decimation counter for goniometer
    int goniometerDecimator { 0 };

    // Linear smoothed gain match multiplier
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGainMatch;
    std::atomic<float> currentGainMatchOffsetDb { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrueLevelAudioProcessor)
};

} // namespace TrueLevel