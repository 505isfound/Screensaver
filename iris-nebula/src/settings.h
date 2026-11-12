// Settings people can change in Screen Saver Settings > Settings...
// Saved for the current Windows user under
// HKEY_CURRENT_USER\Software\Iris Nebula Screensaver
#pragma once
#include <windows.h>
#include "layout.h"

// The sliders. Each goes from 0 to 100; 50 is the normal look.
enum Slider
{
    SliderTwinkleAmount,
    SliderTwinkleSpeed,
    SliderBurstFrequency,
    SliderBurstBrightness,
    SliderGlowAmount,
    SliderEffectSpeed,
    SliderFlashFrequency,
    SliderFlashBrightness,
    SliderSwirlSpeed,
    SliderCount
};

// Number of effect checkboxes (bits in Settings::effects).
const int kEffectCount = 12;
const DWORD kAllEffects = (1u << kEffectCount) - 1;

struct Settings
{
    // Defaults are Keegan's picks (v0.8). Every slider's middle (50) is "100%",
    // which is his chosen look; see GetTuning.
    LayoutMode layout = LayoutMode::Span;
    int  placement = 1;   // 0 nebula centered on main screen, else match wallpaper (see kWallpapers)
    int  sliders[SliderCount] = { 50, 50, 50, 50, 50, 50, 50, 50, 50 };
    bool slowZoom = true;
    DWORD effects = kAllEffects & ~(1u << 10);  // nebula effects (bits, EFFECT_... in shaders.hlsl): all but crackle
};

// What the sliders mean for the drawing code.
struct Tuning
{
    float twinkle;        // twinkle strength (0 = off)
    float twinkleSpeed;   // 1 = normal
    float burstRate;      // how often stars burst, 1 = normal (0 = never)
    float burstGain;      // how bright a burst is, 1 = normal
    float nebula;         // nebula glow and effects strength (0 = off)
    float effectSpeed;    // nebula effects speed, 1 = normal
    float flashInterval;  // average seconds between nebula flashes (0 = never)
    float flashGain;      // how bright a flash is, 1 = normal
    float swirlSpeed;     // how fast the center swirls, 1 = normal
};

// Wallpapers the screensaver can line up with (iris-nebula/wallpapers/).
struct WallpaperMatch
{
    const wchar_t* name;
    float left, top, width, height;   // part of iris.jpg the wallpaper shows
};
extern const WallpaperMatch kWallpapers[];
extern const int kWallpaperCount;

Settings LoadSettings();
void SaveSettings(const Settings& s);
Tuning GetTuning(const Settings& s);

// Shows the settings window. Returns when it is closed.
void ShowSettingsDialog(HINSTANCE instance, HWND parent);
