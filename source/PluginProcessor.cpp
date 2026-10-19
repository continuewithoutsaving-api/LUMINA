#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace TrueLevel
{

juce::AudioProcessorValueTreeState::ParameterLayout TrueLevelAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // 1. Target Platform Preset
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("targetPlatform", 1),
        "Target Platform",
        juce::StringArray {
            "Spotify / YouTube / Apple (-14 LUFS)",
            "Deezer (-15 LUFS)",
            "SoundCloud (-13 LUFS)",
            "Broadcast EBU R128 (-23 LUFS)",
            "Club / CD Master (-9 LUFS)"
        },
        0
    ));

    // 2. Gain Match Toggle
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("gainMatch", 1),
        "Gain Match",
        false
    ));

    // 3. Theme Selector
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("theme", 1),
        "Theme",
        juce::StringArray { "Dark Cyber", "Light Studio", "Vintage Console" },
        0
    ));

    // 4. UI Zoom Scale
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("uiScale", 1),
        "UI Scale",
        juce::StringArray { "75%", "100%", "125%", "150%", "200%" },
        1
    ));

    // 5. Reset Integrated Loudness Trigger
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("resetLufs", 1),
        "Reset Integrated",
        false
    ));

    return { params.begin(), params.end() };
}

TrueLevelAudioProcessor::TrueLevelAudioProcessor()
    : AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", createParameterLayout())
{
    apvts.addParameterListener("targetPlatform", this);
    apvts.addParameterListener("gainMatch", this);
    apvts.addParameterListener("resetLufs", this);
}

TrueLevelAudioProcessor::~TrueLevelAudioProcessor()
{
    apvts.removeParameterListener("targetPlatform", this);
    apvts.removeParameterListener("gainMatch", this);
    apvts.removeParameterListener("resetLufs", this);
}

void TrueLevelAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    meterDSP.prepare(sampleRate, samplesPerBlock);
    channelPointers.resize(2, nullptr);

    smoothedGainMatch.reset(sampleRate, 0.05); // 50ms smooth transition
    smoothedGainMatch.setCurrentAndTargetValue(1.0f);

    goniometerDecimator = 0;
    goniometerQueue.reset();
}

void TrueLevelAudioProcessor::releaseResources()
{
}

bool TrueLevelAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainInputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    return true;
}

float TrueLevelAudioProcessor::getTargetLufs() const noexcept
{
    const int choice = static_cast<int>(apvts.getRawParameterValue("targetPlatform")->load());
    switch (choice)
    {
        case 0: return -14.0f; // Spotify / YouTube / Apple
        case 1: return -15.0f; // Deezer
        case 2: return -13.0f; // SoundCloud
        case 3: return -23.0f; // EBU R128
        case 4: return  -9.0f; // Club / CD Master
        default: return -14.0f;
    }
}

void TrueLevelAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID == "resetLufs" && newValue > 0.5f)
    {
        meterDSP.resetIntegrated();
        if (auto* param = apvts.getParameter("resetLufs"))
            param->setValueNotifyingHost(0.0f);
    }
}

void TrueLevelAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    juce::ScopedNoDenormals noDenormals;
    const int numChannels = buffer.getNumChannels();
    const int numSamples  = buffer.getNumSamples();

    if (numChannels < 2 || numSamples == 0)
        return;

    channelPointers[0] = buffer.getReadPointer(0);
    channelPointers[1] = buffer.getReadPointer(1);

    // 1. Process ITU-R BS.1770-4 LUFS and Stereo Correlation (Zero heap allocation)
    meterDSP.process(channelPointers.data(), 2, numSamples);

    // 2. Feed Goniometer Ring Buffer (Decimated 1:4 to keep GUI CPU under 1%)
    const float* const pL = channelPointers[0];
    const float* const pR = channelPointers[1];
    constexpr float INV_SQRT2 = 0.7071067811865475f;

    for (int i = 0; i < numSamples; ++i)
    {
        if (++goniometerDecimator >= 4)
        {
            goniometerDecimator = 0;
            const float l = pL[i];
            const float r = pR[i];
            GoniometerSample s { (l - r) * INV_SQRT2, (l + r) * INV_SQRT2 };
            goniometerQueue.push(s);
        }
    }

    // 3. Gain Match Processing
    const bool isGainMatchActive = apvts.getRawParameterValue("gainMatch")->load() > 0.5f;
    float targetLinearGain = 1.0f;

    if (isGainMatchActive)
    {
        const float integrated = meterDSP.getIntegratedLufs();
        const float target = getTargetLufs();

        if (integrated > -65.0f)
        {
            const float offsetDb = std::clamp(target - integrated, -36.0f, 12.0f);
            currentGainMatchOffsetDb.store(offsetDb, std::memory_order_relaxed);
            targetLinearGain = std::pow(10.0f, offsetDb / 20.0f);
        }
        else
        {
            currentGainMatchOffsetDb.store(0.0f, std::memory_order_relaxed);
        }
    }
    else
    {
        currentGainMatchOffsetDb.store(0.0f, std::memory_order_relaxed);
    }

    smoothedGainMatch.setTargetValue(targetLinearGain);

    // 4. Apply Gain Smoothing
    if (smoothedGainMatch.isSmoothing())
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float g = smoothedGainMatch.getNextValue();
            buffer.setSample(0, i, buffer.getSample(0, i) * g);
            buffer.setSample(1, i, buffer.getSample(1, i) * g);
        }
    }
    else
    {
        const float g = smoothedGainMatch.getCurrentValue();
        if (std::abs(g - 1.0f) > 0.0001f)
        {
            buffer.applyGain(g);
        }
    }
}

juce::AudioProcessorEditor* TrueLevelAudioProcessor::createEditor()
{
    return new TrueLevelAudioProcessorEditor(*this);
}

void TrueLevelAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void TrueLevelAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

} // namespace TrueLevel

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TrueLevel::TrueLevelAudioProcessor();
}