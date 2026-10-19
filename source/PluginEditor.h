#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "Theme.h"

namespace TrueLevel
{

/**
 * Pure C++ Vector Graphics Goniometer / Vectorscope Component.
 * Visualizes Lissajous stereo trajectories using juce::Graphics vector primitives.
 */
class GoniometerComponent : public juce::Component
{
public:
    explicit GoniometerComponent(TrueLevelAudioProcessor& p);
    ~GoniometerComponent() override = default;

    void paint(juce::Graphics& g) override;
    void pullPointsFromProcessor();
    void setPalette(const ColorPalette& palette);

private:
    TrueLevelAudioProcessor& processor;
    ColorPalette currentPalette;
    std::vector<GoniometerSample> points;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GoniometerComponent)
};

/**
 * Pure C++ LUFS Metering Component.
 * Renders Momentary, Short-Term, and Integrated bars with target reference overlay.
 */
class LufsMeterComponent : public juce::Component
{
public:
    LufsMeterComponent();
    ~LufsMeterComponent() override = default;

    void paint(juce::Graphics& g) override;
    void setLevels(float momentary, float shortTerm, float integrated, float target);
    void setPalette(const ColorPalette& palette);

private:
    float momentaryLufs { -100.0f };
    float shortTermLufs { -100.0f };
    float integratedLufs { -100.0f };
    float targetLufs { -14.0f };
    ColorPalette currentPalette;

    float lufsToYCoordinate(float lufs, float height) const noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LufsMeterComponent)
};

/**
 * Pure C++ Stereo Phase Correlation Component [-1.0 to +1.0].
 */
class CorrelationMeterComponent : public juce::Component
{
public:
    CorrelationMeterComponent();
    ~CorrelationMeterComponent() override = default;

    void paint(juce::Graphics& g) override;
    void setCorrelationValue(float correlation);
    void setPalette(const ColorPalette& palette);

private:
    float correlation { 1.0f };
    ColorPalette currentPalette;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CorrelationMeterComponent)
};

/**
 * Main Plugin Window Editor using standard JUCE C++ UI components.
 * Strictly zero WebBrowserComponent, zero HTML/JS. Pure native desktop rendering.
 */
class TrueLevelAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      public juce::Timer,
                                      public juce::ComboBox::Listener,
                                      public juce::Button::Listener
{
public:
    explicit TrueLevelAudioProcessorEditor(TrueLevelAudioProcessor&);
    ~TrueLevelAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void comboBoxChanged(juce::ComboBox* comboBoxThatHasChanged) override;
    void buttonClicked(juce::Button* button) override;

    void setZoomScale(float scaleFactor);
    void setPalette(ThemePaletteId themeId);

private:
    TrueLevelAudioProcessor& audioProcessor;

    // Sub-components
    GoniometerComponent goniometer;
    LufsMeterComponent lufsMeter;
    CorrelationMeterComponent correlationMeter;

    // Standard Native C++ JUCE Controls
    juce::Label brandLabel;
    juce::ComboBox targetPlatformSelector;
    juce::ComboBox themeSelector;
    juce::ComboBox zoomSelector;
    juce::ToggleButton gainMatchToggle;
    juce::TextButton resetIntegratedButton;

    // Dynamic Numerical Readout Labels
    juce::Label momentaryReadout;
    juce::Label shortTermReadout;
    juce::Label integratedReadout;
    juce::Label gainMatchReadout;

    // Active visual state
    ColorPalette activePalette;
    float currentScale { 1.0f };

    // Layout areas
    juce::Rectangle<int> headerArea;
    juce::Rectangle<int> footerArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrueLevelAudioProcessorEditor)
};

} // namespace TrueLevel