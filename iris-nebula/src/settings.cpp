#include "settings.h"
#include "resource.h"
#include <algorithm>

static const wchar_t* kKey = L"Software\\Iris Nebula Screensaver";

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
    s.twinkle = (int)std::min<DWORD>(ReadNumber(L"Twinkle", 1), 2);
    s.nebula = (int)std::min<DWORD>(ReadNumber(L"NebulaGlow", 1), 2);
    s.slowZoom = ReadNumber(L"SlowZoom", 0) != 0;
    return s;
}

void SaveSettings(const Settings& s)
{
    WriteNumber(L"Layout", (DWORD)s.layout);
    WriteNumber(L"Twinkle", (DWORD)s.twinkle);
    WriteNumber(L"NebulaGlow", (DWORD)s.nebula);
    WriteNumber(L"SlowZoom", s.slowZoom ? 1 : 0);
}

float TwinkleStrength(const Settings& s)
{
    const float values[] = { 0.25f, 0.45f, 0.70f };
    return values[s.twinkle];
}

float NebulaStrength(const Settings& s)
{
    const float values[] = { 0.0f, 1.0f, 2.2f };
    return values[s.nebula];
}

static void FillCombo(HWND dlg, int id, const wchar_t* const* items, int count, int selected)
{
    HWND combo = GetDlgItem(dlg, id);
    for (int i = 0; i < count; i++)
        SendMessageW(combo, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(combo, CB_SETCURSEL, selected, 0);
}

static int ComboChoice(HWND dlg, int id)
{
    LRESULT i = SendDlgItemMessageW(dlg, id, CB_GETCURSEL, 0, 0);
    return i < 0 ? 0 : (int)i;
}

static INT_PTR CALLBACK SettingsProc(HWND dlg, UINT msg, WPARAM wp, LPARAM)
{
    switch (msg)
    {
    case WM_INITDIALOG:
    {
        Settings s = LoadSettings();
        const wchar_t* layouts[] = { L"Automatic", L"One picture across all monitors", L"Each monitor separately" };
        const wchar_t* twinkles[] = { L"Gentle", L"Normal", L"Lively" };
        const wchar_t* glows[] = { L"Off", L"Subtle", L"Stronger" };
        FillCombo(dlg, IDC_LAYOUT, layouts, 3, (int)s.layout);
        FillCombo(dlg, IDC_TWINKLE, twinkles, 3, s.twinkle);
        FillCombo(dlg, IDC_NEBULA, glows, 3, s.nebula);
        CheckDlgButton(dlg, IDC_ZOOM, s.slowZoom ? BST_CHECKED : BST_UNCHECKED);
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wp) == IDOK)
        {
            Settings s;
            s.layout = (LayoutMode)ComboChoice(dlg, IDC_LAYOUT);
            s.twinkle = ComboChoice(dlg, IDC_TWINKLE);
            s.nebula = ComboChoice(dlg, IDC_NEBULA);
            s.slowZoom = IsDlgButtonChecked(dlg, IDC_ZOOM) == BST_CHECKED;
            SaveSettings(s);
            EndDialog(dlg, IDOK);
            return TRUE;
        }
        if (LOWORD(wp) == IDCANCEL)
        {
            EndDialog(dlg, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}

void ShowSettingsDialog(HINSTANCE instance, HWND parent)
{
    DialogBoxW(instance, MAKEINTRESOURCEW(IDD_SETTINGS), parent, SettingsProc);
}
