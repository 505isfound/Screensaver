// Draws the screensaver with Direct3D 11 (built into Windows 10 and 11).
#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include "assets.h"
#include "layout.h"

struct FrameSettings
{
    float time = 0;      // star twinkle clock (seconds, scaled by twinkle speed)
    float nebulaTime = 0; // nebula effects clock (scaled by effect speed)
    float realTime = 0;  // plain seconds since start
    float burstRate = 1;  // star bursts: how often (1 = normal)
    float burstGain = 1;  // star bursts: brightness
    float flashGain = 1;  // nebula flash brightness
    float twinkle = 0;   // strength
    float nebula = 0;    // strength
    float fade = 1;      // 0..1
    float zoom = 1;      // 1 = normal framing
    unsigned effects = 0; // nebula effects (EFFECT_... in shaders.hlsl)
    float flashAge = 1000; // seconds since the nebula last flashed
};

class Renderer
{
public:
    ~Renderer();
    bool Init(HWND window, UINT width, UINT height);
    bool Upload(const Pixels& picture, const Pixels& background, const std::vector<StarRecord>& stars,
                const Layout& layout, const Framing& framing);
    bool Draw(const FrameSettings& frame);   // false if the graphics card was lost

private:
    template <typename T> static void Free(T*& p) { if (p) { p->Release(); p = nullptr; } }
    ID3D11ShaderResourceView* MakeTexture(const Pixels& px);

    UINT width_ = 0, height_ = 0;
    ID3D11Device* device_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
    IDXGISwapChain1* swap_ = nullptr;
    ID3D11RenderTargetView* backView_ = nullptr;

    ID3D11Texture2D* starTex_ = nullptr;
    ID3D11RenderTargetView* starTarget_ = nullptr;
    ID3D11ShaderResourceView* starView_ = nullptr;

    ID3D11VertexShader* starVS_ = nullptr;
    ID3D11PixelShader* starPS_ = nullptr;
    ID3D11VertexShader* compVS_ = nullptr;
    ID3D11PixelShader* compPS_ = nullptr;
    ID3D11Buffer* constants_ = nullptr;
    ID3D11SamplerState* sampler_ = nullptr;
    ID3D11BlendState* addBlend_ = nullptr;
    ID3D11RasterizerState* scissorOn_ = nullptr;

    ID3D11ShaderResourceView* picture_ = nullptr;
    ID3D11ShaderResourceView* background_ = nullptr;
    ID3D11ShaderResourceView* starList_ = nullptr;
    UINT starCount_ = 0;

    Layout layout_;
    Framing framing_ = {};
    bool ready_ = false;
};
