#include "settings.h"
#include "resource.h"
#include "version.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <cwchar>

static const wchar_t* kKey = L"Software\\Iris Nebula Screensaver";

const WallpaperMatch kWallpapers[] = {
    { L"Match wallpaper: Full picture (10711 x 7712)", 0, 0, 10711, 7712 },
    { L"Match wallpaper: Triple 1080p (5760 x 1080)", 752, 4209, 9216, 1728 },
};
const int kWallpaperCount = sizeof(kWallpapers) / sizeof(kWallpapers[0]);

// Registry names of the sliders, in the same order as enum Slider.
static const wchar_t* kSliderNames[SliderCount] = {
    L"TwinkleAmount", L"TwinkleSpeed", L"BurstFrequency", L"BurstBrightness",
    L"GlowAmount", L"EffectSpeed", L"FlashFrequency", L"FlashBrightness", L"SwirlSpeed",
};

static DWORD ReadNumber(const wchar_t* name, DWORD fallback)
{
    DWORD value = 0, size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, kKey, name, RRF_RT_REG_DWORD, nullptr, &value, &size) == ERROR_SUCCESS)
        return value;
    return fallback;
}

static void WriteNumber(const wchar_t* name, DWORD value)
{
    RegSetKeyValueW(HKEY_CURRENT_USER, kKey, name, REG_DWORD, &value, sizeof(value));
}

Settings LoadSettings()
{
    Settings s;
    s.layout = (LayoutMode)std::min<DWORD>(ReadNumber(L"Layout", (DWORD)Settings().layout), 2);
    s.placement = (int)std::min<DWORD>(ReadNumber(L"Placement", (DWORD)Settings().placement), (DWORD)kWallpaperCount);
    // Slider meanings changed in settings version 2 (the middle became Keegan's look),
    // so older saved slider positions are not used.
    if (ReadNumber(L"SettingsVersion", 1) >= 2)
        for (int i = 0; i < SliderCount; i++)
            s.sliders[i] = (int)std::min<DWORD>(ReadNumber(kSliderNames[i], (DWORD)s.sliders[i]), 100);
    s.slowZoom = ReadNumber(L"SlowZoom", 0) != 0;
    s.effects = ReadNumber(L"NebulaEffects", kAllEffects) & kAllEffects;
    // The storm effects (bits 6-10) are new in settings version 3: switch them on.
    DWORD version = ReadNumber(L"SettingsVersion", 1);
    if (version < 3)
        s.effects |= kAllEffects & ~63u;
    // The swirling center (bit 11) is new in settings version 4: switch it on.
    if (version < 4)
        s.effects |= 1u << 11;
    return s;
}

void SaveSettings(const Settings& s)
{
    WriteNumber(L"SettingsVersion", 4);
    WriteNumber(L"Layout", (DWORD)s.layout);
    WriteNumber(L"Placement", (DWORD)s.placement);
    for (int i = 0; i < SliderCount; i++)
        WriteNumber(kSliderNames[i], (DWORD)s.sliders[i]);
    WriteNumber(L"SlowZoom", s.slowZoom ? 1 : 0);
    WriteNumber(L"NebulaEffects", s.effects);
}

Tuning GetTuning(const Settings& s)
{
    auto amount = [&](Slider k) { return s.sliders[k] / 50.0f; };                  // 0 .. 2
    auto speed = [&](Slider k) { return std::pow(2.0f, (s.sliders[k] - 50) / 25.0f); }; // 1/4 .. 4
    Tuning t;
    // The middle of each slider (100%) is the look Keegan picked on 2026-11-12.
    t.twinkle = 0.90f * amount(SliderTwinkleAmount);
    t.twinkleSpeed = 2.0f * speed(SliderTwinkleSpeed);
    t.burstRate = 0.144f * amount(SliderBurstFrequency) * amount(SliderBurstFrequency);  // 0 .. 4x
    t.burstGain = 2.0f * amount(SliderBurstBrightness);
    t.nebula = amount(SliderGlowAmount);
    t.effectSpeed = 2.5f * speed(SliderEffectSpeed);
    t.flashInterval = s.sliders[SliderFlashFrequency] == 0 ? 0.0f
                    : 52.0f * std::pow(2.0f, (50 - s.sliders[SliderFlashFrequency]) / 25.0f);  // ~200 s .. 13 s
    t.flashGain = amount(SliderFlashBrightness);
    t.swirlSpeed = speed(SliderSwirlSpeed);
    return t;
}

// ------------------------------------------------------------- settings window

// Text shown next to a slider: how strong/fast/often compared with normal.
static void ShowPercent(HWND dlg, int slider)
{
    int v = (int)SendDlgItemMessageW(dlg, IDC_SLIDER_FIRST + slider, TBM_GETPOS, 0, 0);
    Settings s;
    s.sliders[slider] = v;
    Settings middle;                       // every slider in the middle = 100%
    for (int& m : middle.sliders)
        m = 50;
    Tuning t = GetTuning(s), normal = GetTuning(middle);
    float ratio = 1.0f;
    switch (slider)
    {
    case SliderTwinkleAmount:   ratio = t.twinkle / normal.twinkle; break;
    case SliderTwinkleSpeed:    ratio = t.twinkleSpeed / normal.twinkleSpeed; break;
    case SliderBurstFrequency:  ratio = t.burstRate / normal.burstRate; break;
    case SliderBurstBrightness: ratio = t.burstGain / normal.burstGain; break;
    case SliderGlowAmount:      ratio = t.nebula / normal.nebula; break;
    case SliderEffectSpeed:     ratio = t.effectSpeed / normal.effectSpeed; break;
    case SliderFlashFrequency:  ratio = t.flashInterval > 0 ? normal.flashInterval / t.flashInterval : 0.0f; break;
    case SliderFlashBrightness: ratio = t.flashGain / normal.flashGain; break;
    case SliderSwirlSpeed:      ratio = t.swirlSpeed / normal.swirlSpeed; break;
    }
    wchar_t text[32];
    if (ratio <= 0.0f)
        wcscpy(text, slider == SliderFlashFrequency ? L"never" : L"off");
    else
        swprintf(text, 32, L"%d%%", (int)(ratio * 100.0f + 0.5f));
    SetDlgItemTextW(dlg, IDC_PERCENT_FIRST + slider, text);
}

static void FillCombo(HWND dlg, int id, const wchar_t* const* items, int count, int selected)
{
    HWND combo = GetDlgItem(dlg, id);
    SendMessageW(combo, CB_RESETCONTENT, 0, 0);
    for (int i = 0; i < count; i++)
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

static void ShowSettings(HWND dlg, const Settings& s)
{
    const wchar_t* layouts[] = { L"Automatic", L"One picture across all monitors", L"Each monitor separately" };
    FillCombo(dlg, IDC_LAYOUT, layouts, 3, (int)s.layout);
    const wchar_t* places[8] = { L"Nebula centered on main screen" };
    for (int i = 0; i < kWallpaperCount && i < 7; i++)
        places[i + 1] = kWallpapers[i].name;
    FillCombo(dlg, IDC_PLACEMENT, places, kWallpaperCount + 1, s.placement);
    for (int i = 0; i < SliderCount; i++)
    {
        HWND slider = GetDlgItem(dlg, IDC_SLIDER_FIRST + i);
        SendMessageW(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(slider, TBM_SETPAGESIZE, 0, 10);
        SendMessageW(slider, TBM_SETTICFREQ, 50, 0);
        SendMessageW(slider, TBM_SETPOS, TRUE, s.sliders[i]);
        ShowPercent(dlg, i);
    }
    for (int i = 0; i < kEffectCount; i++)
        CheckDlgButton(dlg, IDC_EFFECT_FIRST + i, (s.effects >> i) & 1 ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(dlg, IDC_ZOOM, s.slowZoom ? BST_CHECKED : BST_UNCHECKED);
}

static Settings ReadDialog(HWND dlg)
{
    Settings s;
    LRESULT layout = SendDlgItemMessageW(dlg, IDC_LAYOUT, CB_GETCURSEL, 0, 0);
    s.layout = (LayoutMode)(layout < 0 ? 0 : layout);
    LRESULT place = SendDlgItemMessageW(dlg, IDC_PLACEMENT, CB_GETCURSEL, 0, 0);
    s.placement = (int)(place < 0 ? 0 : place);
    for (int i = 0; i < SliderCount; i++)
        s.sliders[i] = (int)SendDlgItemMessageW(dlg, IDC_SLIDER_FIRST + i, TBM_GETPOS, 0, 0);
    s.effects = 0;
    for (int i = 0; i < kEffectCount; i++)
        if (IsDlgButtonChecked(dlg, IDC_EFFECT_FIRST + i) == BST_CHECKED)
            s.effects |= 1u << i;
    s.slowZoom = IsDlgButtonChecked(dlg, IDC_ZOOM) == BST_CHECKED;
    return s;
}

static INT_PTR CALLBACK SettingsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowTextW(dlg, L"Iris Nebula Settings  \u2013  version " IRIS_VERSION);
        ShowSettings(dlg, LoadSettings());
        return TRUE;
    case WM_HSCROLL:
        // A slider moved: update its percent label.
        for (int i = 0; i < SliderCount; i++)
            if ((HWND)lp == GetDlgItem(dlg, IDC_SLIDER_FIRST + i))
                ShowPercent(dlg, i);
        return TRUE;
    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDOK:
            SaveSettings(ReadDialog(dlg));
            EndDialog(dlg, IDOK);
            return TRUE;
        case IDCANCEL:
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        case IDC_DEFAULTS:
            ShowSettings(dlg, Settings());
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowSettingsDialog(HINSTANCE instance, HWND parent)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);
    DialogBoxW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), parent, SettingsProc);
}
