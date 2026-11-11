#pragma once

// Windows reads string 1 as the screensaver's name in Screen Saver Settings.
#define IDS_DESCRIPTION   1

#define IDR_PICTURE       101
#define IDR_BACKGROUND    102
#define IDR_STARS         103
#define IDR_SHADERS       104

#define IDD_SETTINGS      200
#define IDC_LAYOUT        201
#define IDC_ZOOM          204
#define IDC_CREDIT        205
// Nebula effect checkboxes: IDC_EFFECT_FIRST + n is effect bit n.
#define IDC_EFFECT_FIRST  210
// Sliders: IDC_SLIDER_FIRST + n is slider n (enum Slider in settings.h).
#define IDC_SLIDER_FIRST  220
#define IDC_DEFAULTS      240
