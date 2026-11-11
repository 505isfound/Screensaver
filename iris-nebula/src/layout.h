// Works out where the picture goes on each monitor.
#pragma once
#include <windows.h>
#include <vector>

enum class LayoutMode { Auto = 0, Span = 1, EachScreen = 2 };

struct Monitor
{
    RECT rect;       // in window pixels (0,0 = window's top-left)
    bool primary;
};

// One area of the window that shows the picture with its own framing.
struct View
{
    RECT  rect;          // window pixels this view covers
    float focusX, focusY; // where the nebula center goes, in window pixels
    float scale;         // screen pixels per image pixel
};

struct Layout
{
    std::vector<View> views;
    // The part of the image that must be loaded, in image pixels,
    // and how sharp to load it (texture pixels per image pixel, max 1).
    float texLeft = 0, texTop = 0, texWidth = 0, texHeight = 0;
    float texDetail = 1;
};

struct Framing
{
    float imageWidth, imageHeight;
    float focusX, focusY;     // nebula center in image pixels
    float rowsOnScreen;       // image pixels shown across a screen's short side
    float maxZoom;            // 1 = no zoom; >1 leaves room for slow zoom
};

// True when the monitors are all the same size and form a neat rectangle.
bool IsNeatGrid(const std::vector<Monitor>& monitors);

Layout BuildLayout(const std::vector<Monitor>& monitors, LayoutMode mode, const Framing& f);

// Lists monitors relative to the top-left of the whole desktop.
std::vector<Monitor> ListMonitors(RECT* desktopOut);
