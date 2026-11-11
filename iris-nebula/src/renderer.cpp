#include "renderer.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <string>

// Must match "cbuffer ViewConstants" in shaders.hlsl.
struct ViewConstants
{
    float rtSize[2];
    float focusScreen[2];
    float focusImage[2];
    float imageSize[2];
    float texMin[2];
    float texSize[2];
    float scale;
    float time;
    float twinkle;
    float nebula;
    float fade;
    float padding[3];
};
static_assert(sizeof(ViewConstants) % 16 == 0, "constant buffer size");

Renderer::~Renderer()
{
    if (ctx_)
        ctx_->ClearState();
    Free(starList_); Free(background_); Free(picture_);
    Free(scissorOn_); Free(addBlend_); Free(sampler_); Free(constants_);
    Free(compPS_); Free(compVS_); Free(starPS_); Free(starVS_);
    Free(starView_); Free(starTarget_); Free(starTex_);
    Free(backView_); Free(swap_); Free(ctx_); Free(device_);
}

// Turns shader text into code for the graphics card. The result is saved in
// the cache folder, so later starts skip this step.
static std::vector<uint8_t> Compile(const std::string& src, const char* entry, const char* target)
{
    uint32_t hash = 2166136261u;  // FNV-1a hash of the shader text
    for (char c : src)
        hash = (hash ^ (uint8_t)c) * 16777619u;
    wchar_t name[96];
    swprintf(name, 96, L"shader-%hs-%08x.bin", entry, hash);

    std::vector<uint8_t> bytes;
    if (ReadCacheFile(name, &bytes) && !bytes.empty())
        return bytes;

    ID3DBlob* code = nullptr;
    ID3DBlob* errors = nullptr;
    HRESULT hr = D3DCompile(src.data(), src.size(), "shaders.hlsl", nullptr, nullptr, entry, target,
                            D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors)
    {
        OutputDebugStringA((const char*)errors->GetBufferPointer());
        errors->Release();
    }
    if (SUCCEEDED(hr) && code)
    {
        const uint8_t* p = (const uint8_t*)code->GetBufferPointer();
        bytes.assign(p, p + code->GetBufferSize());
        WriteCacheFile(name, bytes.data(), bytes.size());
    }
    if (code)
        code->Release();
    return bytes;
}

bool Renderer::Init(HWND window, UINT width, UINT height)
{
    width_ = width;
    height_ = height;

    D3D_FEATURE_LEVEL level = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &level, 1,
                                   D3D11_SDK_VERSION, &device_, nullptr, &ctx_);
    if (FAILED(hr))  // no suitable graphics card: use Windows' software renderer
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, &level, 1,
                               D3D11_SDK_VERSION, &device_, nullptr, &ctx_);
    if (FAILED(hr))
        return false;

    // Swap chain (the window's drawing surface).
    IDXGIDevice* dxgiDevice = nullptr;
    IDXGIAdapter* adapter = nullptr;
    IDXGIFactory2* factory = nullptr;
    device_->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    if (dxgiDevice) dxgiDevice->GetAdapter(&adapter);
    if (adapter) adapter->GetParent(IID_PPV_ARGS(&factory));
    if (factory)
    {
        DXGI_SWAP_CHAIN_DESC1 sd = {};
        sd.Width = width;
        sd.Height = height;
        sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        sd.SampleDesc.Count = 1;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.BufferCount = 2;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        hr = factory->CreateSwapChainForHwnd(device_, window, &sd, nullptr, nullptr, &swap_);
        if (FAILED(hr))
        {
            sd.BufferCount = 1;
            sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            hr = factory->CreateSwapChainForHwnd(device_, window, &sd, nullptr, nullptr, &swap_);
        }
        factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER);
    }
    if (factory) factory->Release();
    if (adapter) adapter->Release();
    if (dxgiDevice) dxgiDevice->Release();
    if (!swap_)
        return false;

    ID3D11Texture2D* back = nullptr;
    swap_->GetBuffer(0, IID_PPV_ARGS(&back));
    if (!back || FAILED(device_->CreateRenderTargetView(back, nullptr, &backView_)))
    {
        Free(back);
        return false;
    }
    back->Release();

    // Extra layer for star twinkle. Uses a "float" format so stars can get
    // dimmer as well as brighter.
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(device_->CreateTexture2D(&td, nullptr, &starTex_)) ||
        FAILED(device_->CreateRenderTargetView(starTex_, nullptr, &starTarget_)) ||
        FAILED(device_->CreateShaderResourceView(starTex_, nullptr, &starView_)))
        return false;

    // Shaders.
    std::string src = LoadShaderSource();
    std::vector<uint8_t> b;
    if ((b = Compile(src, "StarVS", "vs_5_0")).empty()) return false;
    device_->CreateVertexShader(b.data(), b.size(), nullptr, &starVS_);
    if ((b = Compile(src, "StarPS", "ps_5_0")).empty()) return false;
    device_->CreatePixelShader(b.data(), b.size(), nullptr, &starPS_);
    if ((b = Compile(src, "CompositeVS", "vs_5_0")).empty()) return false;
    device_->CreateVertexShader(b.data(), b.size(), nullptr, &compVS_);
    if ((b = Compile(src, "CompositePS", "ps_5_0")).empty()) return false;
    device_->CreatePixelShader(b.data(), b.size(), nullptr, &compPS_);
    if (!starVS_ || !starPS_ || !compVS_ || !compPS_)
        return false;

    D3D11_BUFFER_DESC cb = {};
    cb.ByteWidth = sizeof(ViewConstants);
    cb.Usage = D3D11_USAGE_DYNAMIC;
    cb.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device_->CreateBuffer(&cb, nullptr, &constants_);

    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&smp, &sampler_);

    D3D11_BLEND_DESC bd = {};
    bd.RenderTarget[0].BlendEnable = TRUE;
    bd.RenderTarget[0].SrcBlend = bd.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha = bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState(&bd, &addBlend_);

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.ScissorEnable = TRUE;
    rd.DepthClipEnable = TRUE;
    device_->CreateRasterizerState(&rd, &scissorOn_);

    return constants_ && sampler_ && addBlend_ && scissorOn_;
}

ID3D11ShaderResourceView* Renderer::MakeTexture(const Pixels& px)
{
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = px.width;
    td.Height = px.height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = { px.bgra.data(), px.width * 4, 0 };
    ID3D11Texture2D* tex = nullptr;
    ID3D11ShaderResourceView* view = nullptr;
    if (SUCCEEDED(device_->CreateTexture2D(&td, &init, &tex)))
        device_->CreateShaderResourceView(tex, nullptr, &view);
    Free(tex);
    return view;
}

bool Renderer::Upload(const Pixels& picture, const Pixels& background, const std::vector<StarRecord>& stars,
                      const Layout& layout, const Framing& framing)
{
    picture_ = MakeTexture(picture);
    background_ = MakeTexture(background);
    if (!picture_ || !background_)
        return false;

    if (!stars.empty())
    {
        // The star list goes into a texture (4096 stars per row) because
        // every graphics card can read those.
        const UINT perRow = 4096;
        UINT rows = (UINT)((stars.size() + perRow - 1) / perRow);
        std::vector<StarRecord> padded(stars);
        padded.resize((size_t)rows * perRow, StarRecord{ -1e6f, -1e6f, 0, 0 });

        D3D11_TEXTURE2D_DESC td = {};
        td.Width = perRow;
        td.Height = rows;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init = { padded.data(), perRow * (UINT)sizeof(StarRecord), 0 };
        ID3D11Texture2D* tex = nullptr;
        if (SUCCEEDED(device_->CreateTexture2D(&td, &init, &tex)) &&
            SUCCEEDED(device_->CreateShaderResourceView(tex, nullptr, &starList_)))
            starCount_ = (UINT)stars.size();
        Free(tex);
    }

    layout_ = layout;
    framing_ = framing;
    ready_ = true;
    return true;
}

bool Renderer::Draw(const FrameSettings& frame)
{
    const float black[4] = { 0, 0, 0, 1 };
    const float zero[4] = { 0, 0, 0, 0 };

    D3D11_VIEWPORT vp = { 0, 0, (float)width_, (float)height_, 0, 1 };
    ctx_->RSSetViewports(1, &vp);
    ctx_->RSSetState(scissorOn_);
    ctx_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx_->IASetInputLayout(nullptr);

    if (!ready_)
    {
        ctx_->OMSetRenderTargets(1, &backView_, nullptr);
        ctx_->ClearRenderTargetView(backView_, black);
    }
    else
    {
        auto setView = [&](const View& v) {
            D3D11_MAPPED_SUBRESOURCE m;
            if (SUCCEEDED(ctx_->Map(constants_, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
            {
                ViewConstants c = {};
                c.rtSize[0] = (float)width_;  c.rtSize[1] = (float)height_;
                c.focusScreen[0] = v.focusX;  c.focusScreen[1] = v.focusY;
                c.focusImage[0] = framing_.focusX;  c.focusImage[1] = framing_.focusY;
                c.imageSize[0] = framing_.imageWidth;  c.imageSize[1] = framing_.imageHeight;
                c.texMin[0] = layout_.texLeft;  c.texMin[1] = layout_.texTop;
                c.texSize[0] = layout_.texWidth;  c.texSize[1] = layout_.texHeight;
                c.scale = v.scale * frame.zoom;
                c.time = frame.time;
                c.twinkle = frame.twinkle;
                c.nebula = frame.nebula;
                c.fade = frame.fade;
                memcpy(m.pData, &c, sizeof(c));
                ctx_->Unmap(constants_, 0);
            }
            ctx_->RSSetScissorRects(1, &v.rect);
        };

        ID3D11ShaderResourceView* none[4] = {};
        ctx_->VSSetConstantBuffers(0, 1, &constants_);
        ctx_->PSSetConstantBuffers(0, 1, &constants_);
        ctx_->PSSetSamplers(0, 1, &sampler_);

        // Step 1: star twinkle layer.
        ctx_->PSSetShaderResources(0, 4, none);
        ctx_->OMSetRenderTargets(1, &starTarget_, nullptr);
        ctx_->ClearRenderTargetView(starTarget_, zero);
        if (starCount_ && frame.twinkle > 0)
        {
            ID3D11ShaderResourceView* in[2] = { picture_, background_ };
            ctx_->PSSetShaderResources(0, 2, in);
            ctx_->VSSetShaderResources(3, 1, &starList_);
            ctx_->VSSetShader(starVS_, nullptr, 0);
            ctx_->PSSetShader(starPS_, nullptr, 0);
            ctx_->OMSetBlendState(addBlend_, nullptr, 0xffffffff);
            for (const auto& v : layout_.views)
            {
                setView(v);
                ctx_->Draw(6 * starCount_, 0);
            }
            ctx_->OMSetBlendState(nullptr, nullptr, 0xffffffff);
        }

        // Step 2: picture + nebula glow + star layer.
        ctx_->OMSetRenderTargets(1, &backView_, nullptr);
        ctx_->ClearRenderTargetView(backView_, black);
        ID3D11ShaderResourceView* in[3] = { picture_, background_, starView_ };
        ctx_->PSSetShaderResources(0, 3, in);
        ctx_->VSSetShader(compVS_, nullptr, 0);
        ctx_->PSSetShader(compPS_, nullptr, 0);
        for (const auto& v : layout_.views)
        {
            setView(v);
            ctx_->Draw(3, 0);
        }
        ctx_->PSSetShaderResources(0, 4, none);
    }

    HRESULT hr = swap_->Present(1, 0);
    return hr != DXGI_ERROR_DEVICE_REMOVED && hr != DXGI_ERROR_DEVICE_RESET;
}
