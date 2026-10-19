#include "PluginEditor.h"

namespace TrueLevel
{

// ==============================================================================
// GoniometerComponent Implementation (Pure juce::Graphics vector rendering)
// ==============================================================================
GoniometerComponent::GoniometerComponent(TrueLevelAudioProcessor& p)
    : processor(p)
{
    points.reserve(1024);
    currentPalette = ColorPalette::get(ThemePaletteId::DarkCyber);
}

void GoniometerComponent::setPalette(const ColorPalette& palette)
{
    currentPalette = palette;
    repaint();
}

void GoniometerComponent::pullPointsFromProcessor()
{
    points.clear();
    GoniometerSample s;
    while (processor.getGoniometerQueue().pop(s))
    {
        points.push_back(s);
        if (points.size() >= 512)
            break;
    }
    repaint();
}

void GoniometerComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float cx = bounds.getCentreX();
    const float cy = bounds.getCentreY();
    const float radius = std::min(cx, cy) - 12.0f;

    // 1. Panel Background & Reticle Scope Cavity
    g.setColour(currentPalette.panelBackground);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(currentPalette.background);
    g.fillEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    // 2. Polar Reticle Vector Grid (Concentric Circles)
    g.setColour(currentPalette.scopeGrid);
    g.drawEllipse(cx - radius * 0.33f, cy - radius * 0.33f, radius * 0.66f, radius * 0.66f, 1.0f);
    g.drawEllipse(cx - radius * 0.66f, cy - radius * 0.66f, radius * 1.32f, radius * 1.32f, 1.0f);
    g.drawEllipse(cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);

    // 3. Orthogonal Axes: Vertical (M: Mid/Mono), Horizontal (S: Side)
    g.drawLine(cx, cy - radius, cx, cy + radius, 1.0f);
    g.drawLine(cx - radius, cy, cx + radius, cy, 1.0f);

    // 4. 45-degree Diagonal Stereo Bounds (L & R)
    const float diag = radius * 0.7071f;
    g.drawLine(cx - diag, cy + diag, cx + diag, cy - diag, 0.8f);
    g.drawLine(cx - diag, cy - diag, cx + diag, cy + diag, 0.8f);

    // 5. Reticle Text Markings
    g.setColour(currentPalette.textDimmed);
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("M", (int)cx - 10, (int)(cy - radius + 2), 20, 14, juce::Justification::centred);
    g.drawText("S", (int)cx - 10, (int)(cy + radius - 16), 20, 14, juce::Justification::centred);
    g.drawText("-S", (int)(cx - radius + 4), (int)cy - 7, 24, 14, juce::Justification::left);
    g.drawText("+S", (int)(cx + radius - 28), (int)cy - 7, 24, 14, juce::Justification::right);
    g.drawText("L", (int)(cx - diag + 6), (int)(cy - diag + 6), 14, 14, juce::Justification::centred);
    g.drawText("R", (int)(cx + diag - 20), (int)(cy - diag + 6), 14, 14, juce::Justification::centred);

    // 6. Draw Vector Scope Lissajous Trace Path
    if (points.size() < 2)
        return;

    const float gain = radius * 1.30f;
    juce::Path tracePath;
    bool isFirst = true;

    for (const auto& pt : points)
    {
        // Coordinate rotation: X = Side, Y = Mid (Y inverted for screen space)
        const float px = cx + std::clamp(pt.x * gain, -radius, radius);
        const float py = cy - std::clamp(pt.y * gain, -radius, radius);

        if (isFirst)
        {
            tracePath.startNewSubPath(px, py);
            isFirst = false;
        }
        else
        {
            tracePath.lineTo(px, py);
        }
    }

    // Outer glow pass
    g.setColour(currentPalette.scopeTrace.withAlpha(0.20f));
    g.strokePath(tracePath, juce::PathStrokeType(2.8f));

    // Core vector stroke pass
    g.setColour(currentPalette.scopeTrace.withAlpha(0.90f));
    g.strokePath(tracePath, juce::PathStrokeType(1.1f));
}

// ==============================================================================
// LufsMeterComponent Implementation
// ==============================================================================
LufsMeterComponent::LufsMeterComponent()
{
    currentPalette = ColorPalette::get(ThemePaletteId::DarkCyber);
}

void LufsMeterComponent::setPalette(const ColorPalette& palette)
{
    currentPalette = palette;
    repaint();
}

void LufsMeterComponent::setLevels(float m, float s, float i, float target)
{
    momentaryLufs = m;
    shortTermLufs = s;
    integratedLufs = i;
    targetLufs = target;
    repaint();
}

float LufsMeterComponent::lufsToYCoordinate(float lufs, float height) const noexcept
{
    constexpr float topLufs = 3.0f;
    constexpr float bottomLufs = -60.0f;
    const float clamped = std::clamp(lufs, bottomLufs, topLufs);
    const float norm = (clamped - bottomLufs) / (topLufs - bottomLufs);
    return height * (1.0f - norm);
}

void LufsMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Background Container
    g.setColour(currentPalette.panelBackground);
    g.fillRoundedRectangle(bounds, 6.0f);

    // Header Label
    g.setColour(currentPalette.textSecondary);
    g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
    g.drawText("LUFS METERS", 10, 8, (int)w - 20, 14, juce::Justification::left);

    const float meterTopY = 28.0f;
    const float meterBottomY = h - 28.0f;
    const float meterHeight = meterBottomY - meterTopY;

    // Ticks: 0, -6, -9, -14, -18, -23, -36, -48
    const float ticks[] = { 0.0f, -6.0f, -9.0f, -14.0f, -18.0f, -23.0f, -36.0f, -48.0f };
    g.setColour(currentPalette.outline);
    g.setFont(juce::FontOptions(9.0f));

    for (float tick : ticks)
    {
        const float ty = meterTopY + lufsToYCoordinate(tick, meterHeight);
        g.drawLine(34.0f, ty, w - 8.0f, ty, 0.5f);
        g.setColour(currentPalette.textDimmed);
        g.drawText(juce::String((int)tick), 2, (int)ty - 6, 28, 12, juce::Justification::centredRight);
        g.setColour(currentPalette.outline);
    }

    // 3 Meter Bars: Momentary (M), Short-Term (S), Integrated (I)
    const float barWidth = 24.0f;
    const float spacing = 12.0f;
    const float startX = 40.0f;

    const float values[3] = { momentaryLufs, shortTermLufs, integratedLufs };
    const juce::String labels[3] = { "M", "S", "I" };

    for (int idx = 0; idx < 3; ++idx)
    {
        const float bx = startX + idx * (barWidth + spacing);
        const float by = meterTopY + lufsToYCoordinate(values[idx], meterHeight);

        // Track cavity
        g.setColour(currentPalette.panelHeader);
        g.fillRoundedRectangle(bx, meterTopY, barWidth, meterHeight, 3.0f);

        // Active fill
        if (values[idx] > -60.0f)
        {
            const juce::Colour fillCol = (values[idx] > targetLufs + 0.5f)
                ? currentPalette.meterOver
                : (idx == 2 ? currentPalette.accentMagenta : currentPalette.meterBase);

            g.setColour(fillCol);
            g.fillRoundedRectangle(bx, by, barWidth, std::max(0.0f, meterBottomY - by), 3.0f);
        }

        // Sub-label
        g.setColour(currentPalette.textSecondary);
        g.setFont(juce::FontOptions(10.0f).withStyle("Bold"));
        g.drawText(labels[idx], (int)bx, (int)h - 22, (int)barWidth, 16, juce::Justification::centred);
    }

    // Target Threshold Line Overlay
    const float targetY = meterTopY + lufsToYCoordinate(targetLufs, meterHeight);
    g.setColour(currentPalette.accentMagenta);
    g.drawLine(startX - 4.0f, targetY, startX + 3.0f * (barWidth + spacing), targetY, 2.0f);
}

// ==============================================================================
// CorrelationMeterComponent Implementation
// ==============================================================================
CorrelationMeterComponent::CorrelationMeterComponent()
{
    currentPalette = ColorPalette::get(ThemePaletteId::DarkCyber);
}

void CorrelationMeterComponent::setPalette(const ColorPalette& palette)
{
    currentPalette = palette;
    repaint();
}

void CorrelationMeterComponent::setCorrelationValue(float c)
{
    correlation = std::clamp(c, -1.0f, 1.0f);
    repaint();
}

void CorrelationMeterComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float w = bounds.getWidth();
    const float h = bounds.getHeight();

    // Track
    g.setColour(currentPalette.panelBackground);
    g.fillRoundedRectangle(bounds, 4.0f);

    const float cx = w * 0.5f;

    // Center divider
    g.setColour(currentPalette.outline);
    g.drawLine(cx, 0.0f, cx, h, 1.0f);

    // Active fill
    const float targetX = cx + (correlation * cx);
    const float barLeft = std::min(cx, targetX);
    const float barWidth = std::abs(targetX - cx);

    const juce::Colour fillCol = (correlation >= 0.0f)
        ? currentPalette.correlationSafe
        : currentPalette.correlationWarning;

    g.setColour(fillCol);
    g.fillRoundedRectangle(barLeft, 2.0f, barWidth, h - 4.0f, 2.0f);

    // Needle Cursor
    g.setColour(currentPalette.textPrimary);
    g.fillRect(targetX - 1.5f, 0.0f, 3.0f, h);

    // Labels
    g.setColour(currentPalette.textDimmed);
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("-1", 6, 0, 16, (int)h, juce::Justification::left);
    g.drawText("0", (int)cx - 6, 0, 12, (int)h, juce::Justification::centred);
    g.drawText("+1", (int)w - 22, 0, 16, (int)h, juce::Justification::right);
}

// ==============================================================================
// TrueLevelAudioProcessorEditor Implementation
// ==============================================================================
TrueLevelAudioProcessorEditor::TrueLevelAudioProcessorEditor(TrueLevelAudioProcessor& p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      goniometer(p)
{
    // Fluid resizable desktop boundaries
    setResizable(true, true);
    setResizeLimits(540, 360, 1920, 1280);
    setSize(720, 480);

    activePalette = ColorPalette::get(ThemePaletteId::DarkCyber);

    // Native Brand Label
    brandLabel.setText("TRUELEVEL", juce::dontSendNotification);
    brandLabel.setFont(juce::FontOptions(17.0f).withStyle("Bold"));
    addAndMakeVisible(brandLabel);

    // Target Platform Selector
    targetPlatformSelector.addItem("Spotify/Apple (-14)", 1);
    targetPlatformSelector.addItem("Deezer (-15)", 2);
    targetPlatformSelector.addItem("SoundCloud (-13)", 3);
    targetPlatformSelector.addItem("EBU R128 (-23)", 4);
    targetPlatformSelector.addItem("Club/CD (-9)", 5);
    targetPlatformSelector.setSelectedId(1, juce::dontSendNotification);
    targetPlatformSelector.addListener(this);
    addAndMakeVisible(targetPlatformSelector);

    // Theme Selector
    themeSelector.addItem("Dark Cyber", 1);
    themeSelector.addItem("Light Studio", 2);
    themeSelector.addItem("Vintage Console", 3);
    themeSelector.setSelectedId(1, juce::dontSendNotification);
    themeSelector.addListener(this);
    addAndMakeVisible(themeSelector);

    // Zoom Selector
    zoomSelector.addItem("75%", 1);
    zoomSelector.addItem("100%", 2);
    zoomSelector.addItem("125%", 3);
    zoomSelector.addItem("150%", 4);
    zoomSelector.addItem("200%", 5);
    zoomSelector.setSelectedId(2, juce::dontSendNotification);
    zoomSelector.addListener(this);
    addAndMakeVisible(zoomSelector);

    // Gain Match Button
    gainMatchToggle.setButtonText("GAIN MATCH");
    gainMatchToggle.addListener(this);
    addAndMakeVisible(gainMatchToggle);

    // Reset Integrated Button
    resetIntegratedButton.setButtonText("RESET");
    resetIntegratedButton.addListener(this);
    addAndMakeVisible(resetIntegratedButton);

    // Numerical Readout Labels
    auto initReadout = [this](juce::Label& lbl) {
        lbl.setFont(juce::FontOptions(13.0f).withStyle("Bold"));
        lbl.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(lbl);
    };
    initReadout(momentaryReadout);
    initReadout(shortTermReadout);
    initReadout(integratedReadout);
    initReadout(gainMatchReadout);

    // Visual Meter Sub-Components
    addAndMakeVisible(goniometer);
    addAndMakeVisible(lufsMeter);
    addAndMakeVisible(correlationMeter);

    setPalette(ThemePaletteId::DarkCyber);

    // Start 60 FPS capped timer for smooth GUI updates
    startTimerHz(60);
}

TrueLevelAudioProcessorEditor::~TrueLevelAudioProcessorEditor()
{
    stopTimer();
}

void TrueLevelAudioProcessorEditor::setPalette(ThemePaletteId themeId)
{
    activePalette = ColorPalette::get(themeId);

    brandLabel.setColour(juce::Label::textColourId, activePalette.textPrimary);
    momentaryReadout.setColour(juce::Label::textColourId, activePalette.meterBase);
    shortTermReadout.setColour(juce::Label::textColourId, activePalette.textPrimary);
    integratedReadout.setColour(juce::Label::textColourId, activePalette.accentMagenta);
    gainMatchReadout.setColour(juce::Label::textColourId, activePalette.accentCyan);

    goniometer.setPalette(activePalette);
    lufsMeter.setPalette(activePalette);
    correlationMeter.setPalette(activePalette);

    repaint();
}

void TrueLevelAudioProcessorEditor::setZoomScale(float scaleFactor)
{
    currentScale = scaleFactor;
    setTransform(juce::AffineTransform::scale(scaleFactor));
}

void TrueLevelAudioProcessorEditor::comboBoxChanged(juce::ComboBox* box)
{
    if (box == &targetPlatformSelector)
    {
        const int idx = targetPlatformSelector.getSelectedId() - 1;
        if (auto* param = audioProcessor.getAPVTS().getParameter("targetPlatform"))
            param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(idx)));
    }
    else if (box == &themeSelector)
    {
        const int idx = themeSelector.getSelectedId() - 1;
        setPalette(static_cast<ThemePaletteId>(idx));
    }
    else if (box == &zoomSelector)
    {
        const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f, 2.0f };
        const int idx = std::clamp(zoomSelector.getSelectedId() - 1, 0, 4);
        setZoomScale(scales[idx]);
    }
}

void TrueLevelAudioProcessorEditor::buttonClicked(juce::Button* btn)
{
    if (btn == &gainMatchToggle)
    {
        if (auto* param = audioProcessor.getAPVTS().getParameter("gainMatch"))
            param->setValueNotifyingHost(gainMatchToggle.getToggleState() ? 1.0f : 0.0f);
    }
    else if (btn == &resetIntegratedButton)
    {
        audioProcessor.getMeterDSP().resetIntegrated();
    }
}

void TrueLevelAudioProcessorEditor::timerCallback()
{
    // 1. Pull goniometer points from lock-free ring buffer
    goniometer.pullPointsFromProcessor();

    // 2. Fetch DSP metrics
    auto& dsp = audioProcessor.getMeterDSP();
    const float m = dsp.getMomentaryLufs();
    const float s = dsp.getShortTermLufs();
    const float i = dsp.getIntegratedLufs();
    const float target = audioProcessor.getTargetLufs();
    const float corr = dsp.getStereoCorrelation();

    lufsMeter.setLevels(m, s, i, target);
    correlationMeter.setCorrelationValue(corr);

    // 3. Update Readout Labels
    momentaryReadout.setText("M: " + (m <= -60.0f ? "-inf" : juce::String(m, 1)) + " LUFS", juce::dontSendNotification);
    shortTermReadout.setText("S: " + (s <= -60.0f ? "-inf" : juce::String(s, 1)) + " LUFS", juce::dontSendNotification);
    integratedReadout.setText("INT: " + (i <= -60.0f ? "-inf" : juce::String(i, 1)) + " LUFS", juce::dontSendNotification);

    const float delta = audioProcessor.getCurrentGainMatchOffsetDb();
    if (gainMatchToggle.getToggleState() && std::abs(delta) > 0.01f)
    {
        gainMatchReadout.setText("MATCH: " + juce::String(delta > 0 ? "+" : "") + juce::String(delta, 1) + " dB", juce::dontSendNotification);
    }
    else
    {
        gainMatchReadout.setText("TARGET: " + juce::String(target, 0) + " LUFS", juce::dontSendNotification);
    }
}

void TrueLevelAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(activePalette.background);

    // Header partition
    g.setColour(activePalette.outline);
    g.drawLine(0.0f, (float)headerArea.getBottom(), (float)getWidth(), (float)headerArea.getBottom(), 1.0f);
}

void TrueLevelAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // 1. Header Toolbar (50px)
    headerArea = area.removeFromTop(50);
    auto h = headerArea.reduced(10, 8);

    brandLabel.setBounds(h.removeFromLeft(110));
    h.removeFromLeft(10);

    targetPlatformSelector.setBounds(h.removeFromLeft(160));
    h.removeFromLeft(8);

    themeSelector.setBounds(h.removeFromLeft(110));
    h.removeFromLeft(8);

    zoomSelector.setBounds(h.removeFromLeft(75));

    resetIntegratedButton.setBounds(h.removeFromRight(65));
    h.removeFromRight(8);

    gainMatchToggle.setBounds(h.removeFromRight(110));

    // 2. Footer Readout Bar (40px)
    footerArea = area.removeFromBottom(40);
    auto f = footerArea.reduced(10, 6);
    const int colW = f.getWidth() / 4;
    momentaryReadout.setBounds(f.removeFromLeft(colW));
    shortTermReadout.setBounds(f.removeFromLeft(colW));
    integratedReadout.setBounds(f.removeFromLeft(colW));
    gainMatchReadout.setBounds(f);

    // 3. Center Section: Goniometer (65%) and LUFS Meters (35%)
    auto centerArea = area.reduced(10, 8);
    const int meterW = std::max(170, static_cast<int>(centerArea.getWidth() * 0.32f));
    auto meterArea = centerArea.removeFromRight(meterW);
    centerArea.removeFromRight(10);

    // Goniometer + Correlation Meter stacked vertically
    auto corrArea = centerArea.removeFromBottom(28);
    centerArea.removeFromBottom(8);

    goniometer.setBounds(centerArea);
    correlationMeter.setBounds(corrArea);
    lufsMeter.setBounds(meterArea);
}

} // namespace TrueLevel