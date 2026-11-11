#include "assets.h"
#include "resource.h"
#include <wincodec.h>
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

bool PictureSize(UINT* width, UINT* height)
{
    IWICImagingFactory* factory;
    IWICBitmapFrameDecode* frame;
    if (!OpenImage(IDR_PICTURE, &factory, &frame))
        return false;
    bool ok = SUCCEEDED(frame->GetSize(width, height));
    frame->Release();
    factory->Release();
    return ok;
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
