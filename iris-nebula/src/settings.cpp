#include "settings.h"
#include "resource.h"
#include "version.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>

static const wchar_t* kKey = L"Software\\Iris Nebula Screensaver";

const WallpaperMatch kWallpapers[] = {
    { L"Match wallpaper: Full picture (10711 x 7712)", 0, 0, 10711, 7712 },
    { L"Match wallpaper: Triple 1080p (5760 x 1080)", 752, 4209, 9216, 1728 },
};
const int kWallpaperCount = sizeof(kWallpapers) / sizeof(kWallpapers[0]);

// Registry names of the sliders, in the same order as enum Slider.
static const wchar_t* kSliderNames[SliderCount] = {
    L"TwinkleAmount", L"TwinkleSpeed", L"BurstFrequency", L"BurstBrightness",
    L"GlowAmount", L"EffectSpeed", L"FlashFrequency", L"FlashBrightness",
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
    s.layout = (LayoutMode)std::min<DWORD>(ReadNumber(L"Layout", 0), 2);
    s.placement = (int)std::min<DWORD>(ReadNumber(L"Placement", 0), (DWORD)kWallpaperCount);
    for (int i = 0; i < SliderCount; i++)
        s.sliders[i] = (int)std::min<DWORD>(ReadNumber(kSliderNames[i], (DWORD)s.sliders[i]), 100);
    s.slowZoom = ReadNumber(L"SlowZoom", 0) != 0;
    s.effects = ReadNumber(L"NebulaEffects", 63) & 63;
    return s;
}

void SaveSettings(const Settings& s)
{
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
    t.twinkle = 0.45f * amount(SliderTwinkleAmount);
    t.twinkleSpeed = speed(SliderTwinkleSpeed);
    t.burstRate = amount(SliderBurstFrequency) * amount(SliderBurstFrequency);     // 0 .. 4
    t.burstGain = amount(SliderBurstBrightness);
    t.nebula = amount(SliderGlowAmount);
    t.effectSpeed = speed(SliderEffectSpeed);
    t.flashInterval = s.sliders[SliderFlashFrequency] == 0 ? 0.0f
                    : 52.0f * std::pow(2.0f, (50 - s.sliders[SliderFlashFrequency]) / 25.0f);  // ~200 s .. 13 s
    t.flashGain = amount(SliderFlashBrightness);
    return t;
}

// ------------------------------------------------------------- settings window

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
    }
    for (int i = 0; i < 6; i++)
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
    for (int i = 0; i < 6; i++)
        if (IsDlgButtonChecked(dlg, IDC_EFFECT_FIRST + i) == BST_CHECKED)
            s.effects |= 1u << i;
    s.slowZoom = IsDlgButtonChecked(dlg, IDC_ZOOM) == BST_CHECKED;
    return s;
}

static INT_PTR CALLBACK SettingsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
{
    switch (msg)
    {
    case WM_INITDIALOG:
        SetWindowTextW(dlg, L"Iris Nebula Settings  \u2013  version " IRIS_VERSION);
        ShowSettings(dlg, LoadSettings());
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
