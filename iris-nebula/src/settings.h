// Settings people can change in Screen Saver Settings > Settings...
// Saved for the current Windows user under
// HKEY_CURRENT_USER\Software\Iris Nebula Screensaver
#pragma once
#include <windows.h>
#include "layout.h"

struct Settings
{
    LayoutMode layout = LayoutMode::Auto;
    int  twinkle = 1;     // 0 gentle, 1 normal, 2 lively
    int  nebula = 1;      // 0 off, 1 subtle, 2 stronger
    bool slowZoom = false;
};

Settings LoadSettings();
void SaveSettings(const Settings& s);

// Shows the settings window. Returns when it is closed.
void ShowSettingsDialog(HINSTANCE instance, HWND parent);

// Values the drawing code uses for each choice.
float TwinkleStrength(const Settings& s);
float NebulaStrength(const Settings& s);
