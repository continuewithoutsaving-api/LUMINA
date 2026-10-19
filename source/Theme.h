#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace TrueLevel
{

enum class ThemePaletteId
{
    DarkCyber = 0,
    LightStudio = 1,
    VintageConsole = 2
};

struct ColorPalette
{
    juce::Colour background;
    juce::Colour panelBackground;
    juce::Colour panelHeader;
    juce::Colour outline;
    juce::Colour textPrimary;
    juce::Colour textSecondary;
    juce::Colour textDimmed;

    juce::Colour meterBase;
    juce::Colour meterTarget;
    juce::Colour meterOver;
    juce::Colour accentCyan;
    juce::Colour accentMagenta;
    juce::Colour scopeTrace;
    juce::Colour scopeGrid;
    juce::Colour correlationSafe;
    juce::Colour correlationWarning;

    static ColorPalette get(ThemePaletteId theme) noexcept
    {
        ColorPalette c;
        switch (theme)
        {
            case ThemePaletteId::DarkCyber:
                c.background         = juce::Colour(0xff0d1117);
                c.panelBackground    = juce::Colour(0xff161b22);
                c.panelHeader        = juce::Colour(0xff21262d);
                c.outline            = juce::Colour(0xff30363d);
                c.textPrimary        = juce::Colour(0xfff0f6fc);
                c.textSecondary      = juce::Colour(0xff8b949e);
                c.textDimmed         = juce::Colour(0xff484f58);
                c.meterBase          = juce::Colour(0xff00f5d4); // Neon Cyan
                c.meterTarget        = juce::Colour(0xff7b2cbf);
                c.meterOver          = juce::Colour(0xffff0054);
                c.accentCyan         = juce::Colour(0xff00f5d4);
                c.accentMagenta      = juce::Colour(0xfff72585);
                c.scopeTrace         = juce::Colour(0xff00f5d4);
                c.scopeGrid          = juce::Colour(0x2800f5d4);
                c.correlationSafe    = juce::Colour(0xff00f5d4);
                c.correlationWarning = juce::Colour(0xffff0054);
                break;

            case ThemePaletteId::LightStudio:
                c.background         = juce::Colour(0xfff5f7fa);
                c.panelBackground    = juce::Colour(0xffffffff);
                c.panelHeader        = juce::Colour(0xffedf2f7);
                c.outline            = juce::Colour(0xffcbd5e1);
                c.textPrimary        = juce::Colour(0xff0f172a);
                c.textSecondary      = juce::Colour(0xff475569);
                c.textDimmed         = juce::Colour(0xff94a3b8);
                c.meterBase          = juce::Colour(0xff2563eb); // Deep Blue
                c.meterTarget        = juce::Colour(0xff0284c7);
                c.meterOver          = juce::Colour(0xffdc2626);
                c.accentCyan         = juce::Colour(0xff0284c7);
                c.accentMagenta      = juce::Colour(0xff4f46e5);
                c.scopeTrace         = juce::Colour(0xff2563eb);
                c.scopeGrid          = juce::Colour(0x302563eb);
                c.correlationSafe    = juce::Colour(0xff16a34a);
                c.correlationWarning = juce::Colour(0xffdc2626);
                break;

            case ThemePaletteId::VintageConsole:
            default:
                c.background         = juce::Colour(0xff1a130f); // Warm walnut
                c.panelBackground    = juce::Colour(0xff271e17);
                c.panelHeader        = juce::Colour(0xff362a20);
                c.outline            = juce::Colour(0xff524133);
                c.textPrimary        = juce::Colour(0xfffed7aa); // Warm ivory
                c.textSecondary      = juce::Colour(0xffc29f79);
                c.textDimmed         = juce::Colour(0xff7c624c);
                c.meterBase          = juce::Colour(0xff22c55e); // Analog VU Green
                c.meterTarget        = juce::Colour(0xfff59e0b); // Analog Amber
                c.meterOver          = juce::Colour(0xffef4444); // Analog Red
                c.accentCyan         = juce::Colour(0xfff59e0b);
                c.accentMagenta      = juce::Colour(0xffea580c);
                c.scopeTrace         = juce::Colour(0xfff59e0b);
                c.scopeGrid          = juce::Colour(0x30f59e0b);
                c.correlationSafe    = juce::Colour(0xff22c55e);
                c.correlationWarning = juce::Colour(0xffef4444);
                break;
        }
        return c;
    }
};

} // namespace TrueLevel