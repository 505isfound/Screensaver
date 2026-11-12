#include "layout.h"
#include <algorithm>
#include <cmath>

static BOOL CALLBACK AddMonitor(HMONITOR m, HDC, LPRECT, LPARAM data)
{
    MONITORINFO info = { sizeof(info) };
    if (GetMonitorInfoW(m, &info))
    {
        Monitor mon;
        mon.rect = info.rcMonitor;
        mon.primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;
        reinterpret_cast<std::vector<Monitor>*>(data)->push_back(mon);
    }
    return TRUE;
}

std::vector<Monitor> ListMonitors(RECT* desktopOut)
{
    std::vector<Monitor> list;
    EnumDisplayMonitors(nullptr, nullptr, AddMonitor, reinterpret_cast<LPARAM>(&list));

    RECT desk = { GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN), 0, 0 };
    desk.right = desk.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    desk.bottom = desk.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    for (auto& m : list)
        OffsetRect(&m.rect, -desk.left, -desk.top);
    if (desktopOut)
        *desktopOut = desk;
    return list;
}

static RECT Bounds(const std::vector<Monitor>& monitors)
{
    RECT b = monitors[0].rect;
    for (const auto& m : monitors)
        UnionRect(&b, &b, &m.rect);
    return b;
}

static long long Area(const RECT& r)
{
    return (long long)(r.right - r.left) * (r.bottom - r.top);
}

bool IsNeatGrid(const std::vector<Monitor>& monitors)
{
    if (monitors.empty())
        return false;
    const LONG w = monitors[0].rect.right - monitors[0].rect.left;
    const LONG h = monitors[0].rect.bottom - monitors[0].rect.top;
    long long total = 0;
    for (const auto& m : monitors)
    {
        if (m.rect.right - m.rect.left != w || m.rect.bottom - m.rect.top != h)
            return false;
        total += Area(m.rect);
    }
    // Windows never lets monitors overlap, so if their areas add up to the
    // bounding box, they fill it with no gaps.
    return total == Area(Bounds(monitors));
}

// Makes one view and makes sure the picture always covers it completely
// (enlarging it if a very large setup needs it).
static View MakeView(const RECT& area, const RECT& focusMonitor, const Framing& f)
{
    View v;
    v.rect = area;
    v.focusX = (focusMonitor.left + focusMonitor.right) * 0.5f;
    v.focusY = (focusMonitor.top + focusMonitor.bottom) * 0.5f;

    float shortSide = (float)std::min(focusMonitor.right - focusMonitor.left,
                                      focusMonitor.bottom - focusMonitor.top);
    float scale = shortSide / f.rowsOnScreen;

    // Smallest scale at which the image still reaches every edge of the view.
    float needLeft = (v.focusX - area.left) / f.focusX;
    float needRight = (area.right - v.focusX) / (f.imageWidth - f.focusX);
    float needTop = (v.focusY - area.top) / f.focusY;
    float needBottom = (area.bottom - v.focusY) / (f.imageHeight - f.focusY);
    scale = std::max({ scale, needLeft, needRight, needTop, needBottom });

    v.scale = scale;
    return v;
}

Layout BuildLayout(const std::vector<Monitor>& monitors, LayoutMode mode, const Framing& f)
{
    Layout layout;
    if (monitors.empty())
        return layout;

    bool span = mode == LayoutMode::Span || (mode == LayoutMode::Auto && IsNeatGrid(monitors));
    if (f.wallWidth > 0 && f.wallHeight > 0)
    {
        // Same as Windows' "Span": the picture fills the box around all
        // monitors, keeping its shape. Whatever doesn't fit is cut off.
        // Measured on Keegan's desktop (5120x1440 + 2560x1440 above): Windows
        // cuts 1/3 of the extra height from the top and 2/3 from the bottom,
        // not half and half. Sideways it is assumed to be half and half.
        const float kCutFromLeft = 0.5f, kCutFromTop = 1.0f / 3.0f;
        RECT box = Bounds(monitors);
        float boxW = (float)(box.right - box.left), boxH = (float)(box.bottom - box.top);
        View v;
        v.rect = box;
        v.scale = std::max(boxW / f.wallWidth, boxH / f.wallHeight);
        float left = box.left + (boxW - f.wallWidth * v.scale) * kCutFromLeft;
        float top = box.top + (boxH - f.wallHeight * v.scale) * kCutFromTop;
        v.focusX = left + (f.focusX - f.wallLeft) * v.scale;
        v.focusY = top + (f.focusY - f.wallTop) * v.scale;
        layout.views.push_back(v);
    }
    else if (span)
    {
        const Monitor* main = &monitors[0];
        for (const auto& m : monitors)
            if (m.primary)
                main = &m;
        layout.views.push_back(MakeView(Bounds(monitors), main->rect, f));
    }
    else
    {
        for (const auto& m : monitors)
            layout.views.push_back(MakeView(m.rect, m.rect, f));
    }

    // Which part of the image is visible anywhere, and how sharp it must be.
    float l = f.imageWidth, t = f.imageHeight, r = 0, b = 0, detail = 0;
    for (const auto& v : layout.views)
    {
        l = std::min(l, f.focusX - (v.focusX - v.rect.left) / v.scale);
        r = std::max(r, f.focusX + (v.rect.right - v.focusX) / v.scale);
        t = std::min(t, f.focusY - (v.focusY - v.rect.top) / v.scale);
        b = std::max(b, f.focusY + (v.rect.bottom - v.focusY) / v.scale);
        detail = std::max(detail, v.scale * f.maxZoom);
    }
    l = std::floor(std::max(l - 2.0f, 0.0f));
    t = std::floor(std::max(t - 2.0f, 0.0f));
    r = std::ceil(std::min(r + 2.0f, f.imageWidth));
    b = std::ceil(std::min(b + 2.0f, f.imageHeight));

    layout.texLeft = l;
    layout.texTop = t;
    layout.texWidth = r - l;
    layout.texHeight = b - t;
    layout.texDetail = std::min(detail, 1.0f);
    return layout;
}
