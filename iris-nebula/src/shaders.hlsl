// Iris Nebula screensaver: GPU drawing code.
//
// Two steps every frame:
//   1. StarVS/StarPS draw one small soft square per star into an "extra light"
//      layer. Each star gets brighter or dimmer over time (twinkle), and the
//      brightest ones sometimes flash their spikes (glint).
//   2. CompositeVS/CompositePS draw the picture, add the star layer, and add a
//      gentle glow to the nebula gas.
//
// Coordinates: "image" means pixels of iris.jpg; "screen" means pixels of the
// window we draw into.

cbuffer ViewConstants : register(b0)
{
    float2 rtSize;        // size of the whole window in pixels
    float2 focusScreen;   // where the nebula center sits on screen
    float2 focusImage;    // nebula center in image pixels
    float2 imageSize;     // full image size in image pixels
    float2 texMin;        // image area held by the picture texture (top-left)
    float2 texSize;       // ... and its size, in image pixels
    float  scale;         // screen pixels per image pixel
    float  time;          // seconds since start
    float  twinkle;       // twinkle strength (0 = off)
    float  nebula;        // nebula glow strength (0 = off)
    float  fade;          // 0..1 fade in at start
    float3 padding;
};

Texture2D<float4>       picture    : register(t0);
Texture2D<float4>       background : register(t1); // picture without stars, small
Texture2D<float4>       starLight  : register(t2); // result of step 1
// Star list stored as a texture, 4096 stars per row: x, y, radius, brightness.
Texture2D<float4>       stars      : register(t3);
SamplerState            linearClamp : register(s0);

static const float TAU = 6.28318530718;

float2 ScreenToImage(float2 screen)
{
    return focusImage + (screen - focusScreen) / scale;
}

float3 SamplePicture(float2 img)
{
    return picture.SampleLevel(linearClamp, (img - texMin) / texSize, 0).rgb;
}

float3 SampleBackground(float2 img)
{
    return background.SampleLevel(linearClamp, img / imageSize, 0).rgb;
}

float Luma(float3 c) { return dot(c, float3(0.3, 0.5, 0.2)); }

// Small repeatable random numbers from a star's number.
float Hash(uint n)
{
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n = n ^ (n >> 4);
    n *= 0x27d4eb2du;
    n = n ^ (n >> 15);
    return (n & 0xFFFFFFu) / 16777216.0;
}

// ---------------------------------------------------------------- step 1: stars

struct StarVertex
{
    float4 pos       : SV_Position;
    float2 center    : CENTER;     // star center on screen
    float  coreSize  : CORE;       // twinkle area radius on screen
    float  quadSize  : QUAD;       // drawn square half-size on screen
    float  amount    : AMOUNT;     // twinkle: -1..+1 times strength
    float  glint     : GLINT;      // spike flash: 0..1
};

StarVertex StarVS(uint id : SV_VertexID)
{
    // 6 corners (two triangles) per star.
    uint starId = id / 6u;
    uint vertexId = id % 6u;
    float4 star = stars.Load(int3(starId % 4096u, starId / 4096u, 0));
    float2 starPos = star.xy;
    float radius = star.z;
    float brightness = star.w;

    // Each star gets its own random speeds and timing.
    float r0 = Hash(starId * 7u + 1u), r1 = Hash(starId * 7u + 2u), r2 = Hash(starId * 7u + 3u);
    float r3 = Hash(starId * 7u + 4u), r4 = Hash(starId * 7u + 5u), r5 = Hash(starId * 7u + 6u);

    // "Activity" slowly comes and goes, so twinkling wanders around the sky
    // instead of every star flickering at once.
    float activity = pow(0.5 + 0.5 * sin(time * TAU / lerp(18.0, 45.0, r0) + r1 * TAU), 3.0);
    // The twinkle itself: two gentle waves mixed together.
    float wave = 0.6 * sin(time * TAU * lerp(0.2, 0.6, r2) + r3 * TAU)
               + 0.4 * sin(time * TAU * lerp(0.5, 1.3, r4) + r5 * TAU);
    float amount = twinkle * activity * wave;

    // The brightest, biggest stars sometimes flash their spikes for a moment.
    bool canGlint = brightness > 0.9 && radius >= 7.0;
    float glint = 0.0;
    if (canGlint && twinkle > 0.0)
    {
        float period = lerp(35.0, 90.0, r1);
        glint = pow(saturate(sin(time * TAU / period + r0 * TAU)), 80.0);
    }

    float core = max((radius * 2.5 + 2.0) * scale, 1.5);
    float quad = canGlint ? max(radius * 10.0 * scale, core) : core;

    float2 center = focusScreen + (starPos - focusImage) * scale;
    float2 corner = float2((vertexId == 1 || vertexId == 2 || vertexId == 4) ? 1.0 : -1.0,
                           (vertexId == 2 || vertexId == 4 || vertexId == 5) ? 1.0 : -1.0);
    float2 screen = center + corner * (quad + 1.0);

    StarVertex o;
    o.pos = float4(screen / rtSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.center = center;
    o.coreSize = core;
    o.quadSize = quad;
    o.amount = amount;
    o.glint = glint;
    return o;
}

float4 StarPS(StarVertex v) : SV_Target
{
    float2 screen = v.pos.xy;
    float dist = length(screen - v.center);
    float2 img = ScreenToImage(screen);

    float3 bg = SampleBackground(img);
    // Only the star's own light, not the gas behind it.
    float3 starOnly = max(SamplePicture(img) - bg * 1.05, 0.0);
    // Stars inside the bright nebula twinkle less, so the gas never flickers.
    float calm = 1.0 - smoothstep(0.30, 0.60, Luma(bg));

    float coreFade = saturate(1.0 - dist / v.coreSize);
    coreFade *= coreFade;
    float spikeFade = saturate(1.0 - dist / v.quadSize);

    float gain = v.amount * coreFade + v.glint * 1.4 * spikeFade;
    return float4(starOnly * gain * calm, 0.0);
}

// ------------------------------------------------------------ step 2: picture

float4 CompositeVS(uint vertexId : SV_VertexID) : SV_Position
{
    // One triangle that covers the whole viewport.
    float2 p = float2((vertexId << 1) & 2, vertexId & 2);
    return float4(p * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
}

float4 CompositePS(float4 pos : SV_Position) : SV_Target
{
    float2 img = ScreenToImage(pos.xy);
    float3 color = SamplePicture(img);
    float3 gas = SampleBackground(img);

    if (nebula > 0.0)
    {
        // How much "nebula" is here: bright gas near the center.
        float d = length(img - focusImage);
        float mask = smoothstep(0.05, 0.45, Luma(gas)) * saturate(1.0 - d / 2600.0);

        // Slow breathing glow.
        float breath = 0.5 + 0.5 * sin(time * TAU / 24.0);
        // A faint wave of light moving outward from the central star,
        // like its light spreading through the dust.
        float cycle = frac(time / 55.0);
        float ring = exp(-pow((d - cycle * 2400.0) / 260.0, 2.0)) * (1.0 - cycle);

        color += gas * mask * nebula * (0.06 * breath + 0.10 * ring);
    }

    color += starLight.Load(int3(pos.xy, 0)).rgb;
    // A tiny bit of noise hides banding in the smooth glow.
    float dither = (Hash((uint)pos.x * 7919u + (uint)pos.y * 104729u) - 0.5) / 255.0;
    return float4(color * fade + dither, 1.0);
}
