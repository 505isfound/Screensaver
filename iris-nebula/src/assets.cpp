#include "assets.h"
#include "resource.h"
#include <wincodec.h>
#include <algorithm>
#include <cstring>

template <typename T> struct Com
{
    T* p = nullptr;
    ~Com() { if (p) p->Release(); }
    T** operator&() { return &p; }
    T* operator->() { return p; }
    operator T*() { return p; }
};

static bool ResourceBytes(int id, const void** data, DWORD* size)
{
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC found = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10) /* RT_RCDATA */);
    if (!found)
        return false;
    HGLOBAL handle = LoadResource(module, found);
    if (!handle)
        return false;
    *data = LockResource(handle);
    *size = SizeofResource(module, found);
    return *data && *size;
}

// Opens a packed image file (JPEG or PNG) with Windows' built-in decoder.
static bool OpenImage(int id, IWICImagingFactory** factoryOut, IWICBitmapFrameDecode** frameOut)
{
    const void* data;
    DWORD size;
    if (!ResourceBytes(id, &data, &size))
        return false;

    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&factory))))
        return false;

    Com<IWICStream> stream;
    Com<IWICBitmapDecoder> decoder;
    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory((BYTE*)data, size)) ||
        FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)))
    {
        factory->Release();
        return false;
    }
    *factoryOut = factory;
    *frameOut = frame;
    return true;
}

static bool CopyAsBGRA(IWICImagingFactory* factory, IWICBitmapSource* source, Pixels* out)
{
    Com<IWICFormatConverter> converter;
    if (FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(source, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone,
                                     nullptr, 0.0, WICBitmapPaletteTypeCustom)))
        return false;
    UINT w, h;
    converter->GetSize(&w, &h);
    out->width = w;
    out->height = h;
    out->bgra.resize((size_t)w * h * 4);
    return SUCCEEDED(converter->CopyPixels(nullptr, w * 4, (UINT)out->bgra.size(), out->bgra.data()));
}

bool LoadPicturePart(float left, float top, float width, float height,
                     UINT outWidth, UINT outHeight, Pixels* out)
{
    IWICImagingFactory* factory;
    IWICBitmapFrameDecode* frame;
    if (!OpenImage(IDR_PICTURE, &factory, &frame))
        return false;

    WICRect part = { (INT)left, (INT)top, (INT)width, (INT)height };
    Com<IWICBitmapClipper> clipper;
    Com<IWICBitmapScaler> scaler;
    bool ok = SUCCEEDED(factory->CreateBitmapClipper(&clipper)) &&
              SUCCEEDED(clipper->Initialize(frame, &part));
    IWICBitmapSource* source = clipper;
    if (ok && (outWidth != (UINT)width || outHeight != (UINT)height))
    {
        ok = SUCCEEDED(factory->CreateBitmapScaler(&scaler)) &&
             SUCCEEDED(scaler->Initialize(clipper, outWidth, outHeight, WICBitmapInterpolationModeFant));
        source = scaler;
    }
    ok = ok && CopyAsBGRA(factory, source, out);
    frame->Release();
    factory->Release();
    return ok;
}

bool LoadBackground(Pixels* out)
{
    IWICImagingFactory* factory;
    IWICBitmapFrameDecode* frame;
    if (!OpenImage(IDR_BACKGROUND, &factory, &frame))
        return false;
    bool ok = CopyAsBGRA(factory, frame, out);
    frame->Release();
    factory->Release();
    return ok;
}

bool LoadStars(std::vector<StarRecord>* out)
{
    const void* data;
    DWORD size;
    if (!ResourceBytes(IDR_STARS, &data, &size) || size < 8 || memcmp(data, "STAR", 4) != 0)
        return false;
    uint32_t count;
    memcpy(&count, (const uint8_t*)data + 4, 4);
    if (8 + (size_t)count * sizeof(StarRecord) > size)
        return false;
    out->resize(count);
    memcpy(out->data(), (const uint8_t*)data + 8, (size_t)count * sizeof(StarRecord));
    return true;
}

std::string LoadShaderSource()
{
    const void* data;
    DWORD size;
    if (!ResourceBytes(IDR_SHADERS, &data, &size))
        return {};
    return std::string((const char*)data, size);
}

// ---------------------------------------------------------------- picture cache

// Change this number whenever the packed picture or the cache format changes.
static const uint32_t kCacheVersion = 1;

struct CacheHeader
{
    char magic[8];             // "IRISNEB\0"
    uint32_t version;
    uint32_t pictureBytes;     // size of the packed picture: changes if it is replaced
    PictureCacheKey key;
    uint32_t width, height;    // pixel size of the saved copy
};

static std::wstring CacheFolder(bool create)
{
    wchar_t base[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    std::wstring folder = std::wstring(base) + L"\\Iris Nebula Screensaver";
    if (create)
        CreateDirectoryW(folder.c_str(), nullptr);
    return folder;
}

static std::wstring CachePath(const wchar_t* slot, bool createFolder)
{
    std::wstring folder = CacheFolder(createFolder);
    return folder.empty() ? folder : folder + L"\\picture-" + slot + L".cache";
}

bool ReadCacheFile(const wchar_t* name, std::vector<uint8_t>* out)
{
    std::wstring folder = CacheFolder(false);
    if (folder.empty())
        return false;
    HANDLE f = CreateFileW((folder + L"\\" + name).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER size;
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (16 << 20);
    if (ok)
    {
        out->resize((size_t)size.QuadPart);
        DWORD got = 0;
        ok = ReadFile(f, out->data(), (DWORD)out->size(), &got, nullptr) && got == out->size();
    }
    CloseHandle(f);
    return ok;
}

void WriteCacheFile(const wchar_t* name, const void* data, size_t size)
{
    std::wstring folder = CacheFolder(true);
    if (folder.empty())
        return;
    std::wstring path = folder + L"\\" + name, temp = path + L".tmp";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;
    DWORD wrote = 0;
    bool ok = WriteFile(f, data, (DWORD)size, &wrote, nullptr) && wrote == size;
    CloseHandle(f);
    if (!ok || !MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING))
        DeleteFileW(temp.c_str());
}

static uint32_t PictureBytes()
{
    const void* data;
    DWORD size;
    return ResourceBytes(IDR_PICTURE, &data, &size) ? size : 0;
}

static bool SameKey(const PictureCacheKey& a, const PictureCacheKey& b)
{
    return a.left == b.left && a.top == b.top && a.width == b.width && a.height == b.height &&
           a.outWidth == b.outWidth && a.outHeight == b.outHeight;
}

bool LoadCachedPicture(const wchar_t* slot, const PictureCacheKey& key, Pixels* out)
{
    std::wstring path = CachePath(slot, false);
    if (path.empty())
        return false;
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return false;

    bool ok = false;
    CacheHeader h;
    DWORD got = 0;
    LARGE_INTEGER fileSize;
    if (ReadFile(f, &h, sizeof(h), &got, nullptr) && got == sizeof(h) &&
        memcmp(h.magic, "IRISNEB", 8) == 0 && h.version == kCacheVersion &&
        h.pictureBytes == PictureBytes() && SameKey(h.key, key) &&
        h.width == key.outWidth && h.height == key.outHeight &&
        GetFileSizeEx(f, &fileSize) &&
        (uint64_t)fileSize.QuadPart == sizeof(h) + (uint64_t)h.width * h.height * 4)
    {
        out->width = h.width;
        out->height = h.height;
        out->bgra.resize((size_t)h.width * h.height * 4);
        // Read in pieces (ReadFile takes at most 4 GB at a time).
        ok = true;
        size_t done = 0;
        while (ok && done < out->bgra.size())
        {
            DWORD chunk = (DWORD)std::min<size_t>(out->bgra.size() - done, 64u << 20);
            ok = ReadFile(f, out->bgra.data() + done, chunk, &got, nullptr) && got == chunk;
            done += chunk;
        }
        if (!ok)
            out->bgra.clear();
    }
    CloseHandle(f);
    return ok;
}

void SaveCachedPicture(const wchar_t* slot, const PictureCacheKey& key, const Pixels& px)
{
    std::wstring path = CachePath(slot, true);
    if (path.empty())
        return;
    // Write to a temporary file first so a half-written copy is never used.
    std::wstring temp = path + L".tmp";
    HANDLE f = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE)
        return;

    CacheHeader h = {};
    memcpy(h.magic, "IRISNEB", 8);
    h.version = kCacheVersion;
    h.pictureBytes = PictureBytes();
    h.key = key;
    h.width = px.width;
    h.height = px.height;
    DWORD wrote = 0;
    bool ok = WriteFile(f, &h, sizeof(h), &wrote, nullptr) && wrote == sizeof(h);
    size_t done = 0;
    while (ok && done < px.bgra.size())
    {
        DWORD chunk = (DWORD)std::min<size_t>(px.bgra.size() - done, 64u << 20);
        ok = WriteFile(f, px.bgra.data() + done, chunk, &wrote, nullptr) && wrote == chunk;
        done += chunk;
    }
    CloseHandle(f);
    if (ok)
        ok = MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
    if (!ok)
        DeleteFileW(temp.c_str());
}
