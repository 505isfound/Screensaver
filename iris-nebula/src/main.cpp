// Iris Nebula screensaver.
//
// Windows starts a screensaver (.scr file) in one of three ways:
//   /s            run full screen
//   /p <window>   draw inside the small preview monitor in Screen Saver Settings
//   /c or nothing show the Settings window
#include <windows.h>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <random>
#include <thread>
#include <algorithm>
#include "assets.h"
#include "layout.h"
#include "renderer.h"
#include "settings.h"

// --- Framing of the picture -------------------------------------------------
// Size of assets/iris.jpg (printed by tools/prepare_image.py). Written here so
// starting up doesn't have to open the big picture.
static const float kImageWidth = 10711.0f;
static const float kImageHeight = 10170.0f;
// Nebula center (the bright star HD 200775) in iris.jpg pixels.
static const float kFocusX = 5360.0f;
static const float kFocusY = 5073.0f;
// How many image pixels fit across the short side of the main screen.
// 1728 matches the "zoomed out" look picked for a 1080p screen.
static const float kRowsOnScreen = 1728.0f;
// Slow zoom (only when turned on in Settings): how far, and how slowly.
static const float kZoomAmount = 1.15f;
static const float kZoomSeconds = 300.0f;
// Largest picture we load onto the graphics card.
static const double kMaxTexturePixels = 64.0e6;
static const UINT kMaxTextureSide = 16384;

enum class Mode { Settings, Preview, Saver };

struct Loaded
{
    Pixels picture, background;
    std::vector<StarRecord> stars;
    std::atomic<int> state{ 0 };   // 0 loading, 1 done, -1 failed
};

static POINT g_startCursor;
static bool g_haveStartCursor = false;
static bool g_closeOnInput = false;

static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_SETCURSOR:
        if (g_closeOnInput)
        {
            SetCursor(nullptr);
            return TRUE;
        }
        break;
    case WM_MOUSEMOVE:
        if (g_closeOnInput)
        {
            POINT p;
            GetCursorPos(&p);
            if (!g_haveStartCursor)
            {
                g_startCursor = p;
                g_haveStartCursor = true;
            }
            else if (std::abs(p.x - g_startCursor.x) > 10 || std::abs(p.y - g_startCursor.y) > 10)
                PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
        break;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
    case WM_MOUSEWHEEL:
        if (g_closeOnInput)
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;
    case WM_ACTIVATEAPP:
        if (g_closeOnInput && !wp)
            PostMessageW(hwnd, WM_CLOSE, 0, 0);
        break;
    case WM_SYSCOMMAND:
        // Don't let Windows start the screensaver again on top of itself.
        if ((wp & 0xFFF0) == SC_SCREENSAVE)
            return 0;
        break;
    case WM_ERASEBKGND:
        return 1;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

static void UseRealPixels()
{
    // Work in real screen pixels on every monitor, even with Windows scaling.
    typedef BOOL(WINAPI * SetContextFn)(HANDLE);
    auto setContext = (SetContextFn)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                   "SetProcessDpiAwarenessContext");
    if (!setContext || !setContext((HANDLE)-4 /* PER_MONITOR_AWARE_V2 */))
        SetProcessDPIAware();
}

static HWND ParseWindow(const wchar_t* s)
{
    while (*s == L' ' || *s == L':')
        s++;
    return (HWND)(UINT_PTR)wcstoull(s, nullptr, 10);
}

static int RunScreensaver(HINSTANCE instance, Mode mode, HWND previewParent)
{
    Settings settings = LoadSettings();

    // Where the window goes and which monitors it covers.
    std::vector<Monitor> monitors;
    RECT area = {};
    if (mode == Mode::Preview)
    {
        GetClientRect(previewParent, &area);
        monitors.push_back({ area, true });
    }
    else
    {
        monitors = ListMonitors(&area);
    }
    const UINT width = std::max<LONG>(area.right - area.left, 1);
    const UINT height = std::max<LONG>(area.bottom - area.top, 1);

    Framing framing;
    framing.imageWidth = kImageWidth;
    framing.imageHeight = kImageHeight;
    framing.focusX = kFocusX;
    framing.focusY = kFocusY;
    framing.rowsOnScreen = kRowsOnScreen;
    framing.maxZoom = settings.slowZoom ? kZoomAmount : 1.0f;
    if (settings.placement > 0)
    {
        const WallpaperMatch& w = kWallpapers[settings.placement - 1];
        framing.wallLeft = w.left;
        framing.wallTop = w.top;
        framing.wallWidth = w.width;
        framing.wallHeight = w.height;
    }
    Layout layout = BuildLayout(monitors, mode == Mode::Preview ? LayoutMode::EachScreen : settings.layout, framing);

    // Window.
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = L"IrisNebulaScreensaver";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    HWND hwnd;
    if (mode == Mode::Preview)
    {
        hwnd = CreateWindowExW(0, wc.lpszClassName, L"", WS_CHILD | WS_VISIBLE,
                               0, 0, width, height, previewParent, nullptr, instance, nullptr);
    }
    else
    {
        // Starts hidden: it only appears once the picture is ready,
        // so the desktop switches straight to the full picture.
        g_closeOnInput = true;
        hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, wc.lpszClassName, L"Iris Nebula",
                               WS_POPUP, area.left, area.top, width, height,
                               nullptr, nullptr, instance, nullptr);
    }
    if (!hwnd)
        return 1;

    bool shown = mode == Mode::Preview;
    auto show = [&] {
        if (shown)
            return;
        shown = true;
        ShowWindow(hwnd, SW_SHOW);
        SetForegroundWindow(hwnd);
        ShowCursor(FALSE);
    };
    // Any mouse or keyboard use while still loading means "stop".
    LASTINPUTINFO lastInput = { sizeof(lastInput) };
    GetLastInputInfo(&lastInput);
    const DWORD inputAtStart = lastInput.dwTime;

    Renderer renderer;
    if (!renderer.Init(hwnd, width, height))
    {
        // Graphics failed: still behave like a (black) screensaver.
        show();
        MSG msg;
        while (GetMessageW(&msg, nullptr, 0, 0) > 0)
        {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return 0;
    }

    // Decide how sharp to load the picture, then load it in the background.
    double tw = layout.texWidth * layout.texDetail, th = layout.texHeight * layout.texDetail;
    double shrink = std::min({ 1.0, kMaxTextureSide / tw, kMaxTextureSide / th,
                               std::sqrt(kMaxTexturePixels / (tw * th)) });
    UINT texW = std::max<UINT>(1, (UINT)(tw * shrink));
    UINT texH = std::max<UINT>(1, (UINT)(th * shrink));

    Loaded loaded;
    std::thread loader([&] {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        // Use the ready-made copy from last time if it matches this setup;
        // otherwise unpack the big picture (slow) and save a copy for next time.
        PictureCacheKey key = { layout.texLeft, layout.texTop, layout.texWidth, layout.texHeight, texW, texH };
        const wchar_t* slot = mode == Mode::Preview ? L"preview" : L"screen";
        bool ok = LoadCachedPicture(slot, key, &loaded.picture);
        if (!ok)
        {
            ok = LoadPicturePart(layout.texLeft, layout.texTop, layout.texWidth, layout.texHeight,
                                 texW, texH, &loaded.picture);
            if (ok)
                SaveCachedPicture(slot, key, loaded.picture);
        }
        ok = ok && LoadBackground(&loaded.background);
        if (ok)
            LoadStars(&loaded.stars);  // no stars is fine: just no twinkle
        loaded.state = ok ? 1 : -1;
        CoUninitialize();
    });

    LARGE_INTEGER freq, start, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&start);
    bool uploaded = false;
    std::mt19937 rng(GetTickCount() ^ GetCurrentProcessId());
    float lastFlash = -1000.0f;
    float nextFlash = 4.0f + 8.0f * (rng() / (float)rng.max());   // first one soon after start

    // For making preview videos on slow test computers: IRIS_TIME_SCALE=0.25
    // runs everything 4 times slower so the recording can be sped up smoothly.
    float timeScale = 1.0f;
    wchar_t scaleText[32];
    if (GetEnvironmentVariableW(L"IRIS_TIME_SCALE", scaleText, 32) > 0)
        timeScale = std::min(std::max((float)_wtof(scaleText), 0.01f), 10.0f);

    FrameSettings frame;
    const Tuning tuning = GetTuning(settings);
    frame.twinkle = tuning.twinkle;
    frame.nebula = tuning.nebula;
    frame.burstRate = tuning.burstRate;
    frame.burstGain = tuning.burstGain;
    frame.flashGain = tuning.flashGain;
    float lastT = 0, starClock = 0, nebulaClock = 0;
    frame.effects = settings.effects;

    MSG msg = {};
    bool running = true;
    while (running)
    {
        while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
                running = false;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!running)
            break;

        QueryPerformanceCounter(&now);
        float t = (float)((now.QuadPart - start.QuadPart) / (double)freq.QuadPart);
        t *= timeScale;

        if (!uploaded)
        {
            if (loaded.state == 1)
            {
                uploaded = renderer.Upload(loaded.picture, loaded.background, loaded.stars, layout, framing);
                loaded.picture.bgra = {};
                loaded.background.bgra = {};
                if (!uploaded)
                    loaded.state = -1;
            }
            if (loaded.state == 0)
            {
                if (g_closeOnInput && GetLastInputInfo(&lastInput) && lastInput.dwTime != inputAtStart)
                    break;
                Sleep(5);
                continue;
            }
            // If loading failed we carry on and show a black screen.
        }

        // The nebula flashes at random moments: on average every
        // tuning.flashInterval seconds, anywhere from half to 1.5 times that.
        if (tuning.flashInterval > 0 && t >= nextFlash)
        {
            lastFlash = nextFlash;
            nextFlash = t + tuning.flashInterval * (0.5f + rng() / (float)rng.max());
        }
        frame.flashAge = t - lastFlash;

        // Separate clocks so "speed" sliders change speed without jumps.
        float dt = t - lastT;
        lastT = t;
        starClock += dt * tuning.twinkleSpeed;
        nebulaClock += dt * tuning.effectSpeed;
        frame.time = starClock;
        frame.nebulaTime = nebulaClock;
        frame.realTime = t;
        frame.fade = 1;
        frame.zoom = 1 + (framing.maxZoom - 1) * (0.5f - 0.5f * std::cos(t * 6.2831853f / kZoomSeconds));
        if (!renderer.Draw(frame))
            DestroyWindow(hwnd);   // graphics card was reset: just close
        show();                    // after the first finished frame
    }

    loader.join();
    return 0;
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR cmd, int)
{
    UseRealPixels();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    Mode mode = Mode::Settings;
    HWND window = nullptr;
    const wchar_t* p = cmd;
    while (*p == L' ' || *p == L'"')
        p++;
    if (*p == L'/' || *p == L'-')
    {
        wchar_t c = (wchar_t)towlower(p[1]);
        if (c == L's')
            mode = Mode::Saver;
        else if (c == L'p')
        {
            mode = Mode::Preview;
            window = ParseWindow(p + 2);
        }
        else if (c == L'c')
            window = ParseWindow(p + 2);
    }

    int result = 0;
    if (mode == Mode::Settings)
        ShowSettingsDialog(instance, window ? window : GetForegroundWindow());
    else if (mode == Mode::Preview && !IsWindow(window))
        result = 1;
    else
        result = RunScreensaver(instance, mode, window);

    CoUninitialize();
    return result;
}
