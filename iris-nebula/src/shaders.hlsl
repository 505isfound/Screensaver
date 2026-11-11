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
    float  time;          // clock for star twinkle (runs faster/slower with "Twinkle speed")
    float  twinkle;       // twinkle strength (0 = off)
    float  nebula;        // nebula glow strength (0 = off)
    float  fade;          // 0..1 fade in at start
    uint   effects;       // which nebula effects are on (EFFECT_... below)
    float  flashAge;      // seconds since the nebula's last flash (random timing, set by the program)
    float  nebulaTime;    // clock for nebula effects (runs faster/slower with "Effect speed")
    float  burstRate;     // how often stars burst, 1 = normal
    float  burstGain;     // how bright bursts are, 1 = normal
    float  flashGain;     // how bright the nebula flash is, 1 = normal
    float  realTime;      // plain clock in seconds (for random star events)
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

// Smooth random value 0..1 that wanders over time. Unlike a sine wave it never
// repeats, so there are no patterns to spot. Each seed gives a different path.
float TimeNoise(float x, uint seed)
{
    float i = floor(x);
    float f = x - i;
    f = f * f * (3.0 - 2.0 * f);
    uint k = (uint)i;
    return lerp(Hash(k * 747796405u + seed), Hash((k + 1u) * 747796405u + seed), f);
}

// Random one-off events: time is cut into slots, and in each slot the event
// happens with the given chance, at a random moment. Returns 0..1:
// quick rise to 1, then a fall back to 0.
float RandomEvent(uint seed, float slotLength, float chance, float riseTime, float fallTime)
{
    float slots = realTime / slotLength;
    uint k = (uint)floor(slots);
    float level = 0.0;
    // This slot and the one before (an event late in the last slot may still be fading).
    for (uint j = 0u; j < 2u; j++)
    {
        if (k < j)
            break;
        uint kk = k - j;
        uint h = kk * 2654435761u ^ seed * 40503u;
        if (Hash(h) < chance)
        {
            float age = realTime - (kk + Hash(h + 977u)) * slotLength;
            if (age >= 0.0)
            {
                float v = age < riseTime ? smoothstep(0.0, 1.0, age / riseTime)
                                         : exp(-(age - riseTime) / fallTime);
                level = max(level, v);
            }
        }
    }
    return level;
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
    float r0 = Hash(starId * 7u + 1u), r2 = Hash(starId * 7u + 3u), r4 = Hash(starId * 7u + 5u);

    // "Activity" wanders up and down, so twinkling moves around the sky
    // instead of every star flickering at once (same amount as v0.1, but random timing).
    float activity = pow(saturate((TimeNoise(time / lerp(9.0, 22.0, r0), starId * 3u + 11u) - 0.1) / 0.8), 3.0);
    // The twinkle itself: two random wobbles, a slow one and a quicker one.
    float wave = (TimeNoise(time * lerp(0.4, 0.9, r2), starId * 3u + 12u) * 2.0 - 1.0) * 1.1
               + (TimeNoise(time * lerp(1.0, 2.2, r4), starId * 3u + 13u) * 2.0 - 1.0) * 0.6;
    float amount = twinkle * activity * wave;

    // The brightest, biggest stars now and then flash their spikes.
    bool canGlint = brightness > 0.9 && radius >= 7.0;
    float glint = 0.0;
    if (canGlint && twinkle > 0.0)
        glint = RandomEvent(starId * 5u + 1u, 30.0, 0.5, 0.35, 0.7);

    // A small share of stars give a quick bright burst: a fraction of a second
    // up, then straight back to normal. On a 1080p screen about one every
    // two seconds, each at a random star and a random moment.
    float burst = 0.0;
    if (burstRate > 0.0 && brightness > 0.6)
        burst = RandomEvent(starId * 5u + 2u, 8.0, min(0.0012 * burstRate, 1.0), 0.06, 0.10) * burstGain;
    glint = max(glint, burst);
    amount += burst * 2.2;

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

// Nebula effects that can be switched on and off.
#define EFFECT_DRIFT    1u   // gas slowly flows and curls
#define EFFECT_ECHO     2u   // central star pulses, light spreads outward
#define EFFECT_SHADOWS  4u   // dust shadows drift across
#define EFFECT_SHIMMER  8u   // blue glow shifts slightly in tone
#define EFFECT_RAYS     16u  // faint light beams from the central star
#define EFFECT_NEWSTARS 32u  // a few spots inside the gas softly pulse

// Smooth random "noise" (values 0..1 that change gradually across space).
float Hash2(float2 p)
{
    p = frac(p * float2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return frac(p.x * p.y);
}

float Noise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = Hash2(i), b = Hash2(i + float2(1, 0)), c = Hash2(i + float2(0, 1)), d = Hash2(i + float2(1, 1));
    return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

// Angle of a direction, -pi..pi (written out because some shader compilers lack atan2).
float Angle(float2 v)
{
    float ax = abs(v.x), ay = abs(v.y);
    float a = min(ax, ay) / max(max(ax, ay), 1e-6);
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    if (ay > ax) r = 1.57079637 - r;
    if (v.x < 0) r = 3.14159274 - r;
    if (v.y < 0) r = -r;
    return r;
}

float Clouds(float2 p)
{
    return (Noise(p) * 0.5 + Noise(p * 2.03 + 17.1) * 0.3 + Noise(p * 4.1 + 41.7) * 0.2);
}

// Direction the gas flows at a spot: swirls plus a slow drift outward.
float2 Flow(float2 img)
{
    float2 p = img / 700.0 + float2(nebulaTime * 0.004, 0.0);
    float e = 0.05;
    float n1 = Clouds(p + float2(0, e)), n2 = Clouds(p - float2(0, e));
    float n3 = Clouds(p + float2(e, 0)), n4 = Clouds(p - float2(e, 0));
    float2 swirl = float2(n1 - n2, n4 - n3) / (2.0 * e);   // "curl": flows around, never piles up
    float2 out_ = normalize(img - focusImage + 0.001);
    return swirl * 0.9 + out_ * 0.25;
}

// How much of a pixel is star (bright dot above the gas) rather than gas.
float Starness(float3 c, float3 gas)
{
    return saturate((Luma(c) - Luma(gas)) * 6.0);
}

// Spots inside the nebula that act as newborn stars (picture pixels).
static const float2 kNewStars[8] =
{
    float2(5147, 5393), float2(4987, 5097), float2(5419, 5497), float2(5299, 5617),
    float2(5827, 5064), float2(6348, 5377), float2(5459, 4112), float2(6003, 4992)
};

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
        float wide = smoothstep(0.02, 0.12, Luma(gas)) * saturate(1.0 - d / 3800.0);
        float strength = saturate(nebula);   // 1 for Subtle and Stronger; Stronger also scales below

        // 1. Drifting gas. Two copies of the picture slide along the flow and
        //    take turns, so the gas keeps moving without ever tearing.
        if ((effects & EFFECT_DRIFT) && wide > 0.0)
        {
            const float period = 16.0;
            float2 flow = Flow(img) * 0.45 * nebula;          // picture pixels per second
            // Each area has its own timing so the whole nebula never "pulses" together.
            float offset = Noise(img / 1800.0 + 3.7);
            float ph0 = frac(nebulaTime / period + offset), ph1 = frac(nebulaTime / period + offset + 0.5);
            float3 a = SamplePicture(img - flow * ph0 * period);
            float3 b = SamplePicture(img - flow * ph1 * period);
            float3 moved = lerp(a, b, abs(ph0 - 0.5) * 2.0);
            // Only the gas moves: stars stay sharp and still.
            float keep = max(Starness(color, gas), Starness(moved, gas));
            color = lerp(color, moved, wide * (1.0 - keep));
        }

        // 3. Passing dust shadows (in front of everything, so stars dim too).
        if (effects & EFFECT_SHADOWS)
        {
            float2 p = img / 950.0 + float2(nebulaTime * 0.010, nebulaTime * -0.006);
            float shade = smoothstep(0.50, 0.78, Clouds(p));
            color *= 1.0 - shade * 0.22 * nebula * wide;
        }

        // Gentle breathing glow (always on).
        float breath = TimeNoise(nebulaTime / 14.0, 101u);
        float glow = 0.06 * breath;

        // 2. Light echo: the central star brightens, and the light travels
        //    outward through the gas over about 14 seconds.
        if (effects & EFFECT_ECHO)
        {
            float t = flashAge;                              // seconds since the flash
            float travel = saturate(t / 14.0);               // ring moves outward for 14 s
            float ring = exp(-pow((d - travel * 2600.0) / 320.0, 2.0)) * (1.0 - travel);
            // The flash: a split-second peak that drops to about half within
            // a fraction of a second, then fades back to normal over several seconds.
            float rise = smoothstep(0.0, 0.06, t);
            float flashLevel = rise * (0.5 * exp(-t / 0.12) + 0.5 * exp(-t / 5.0));
            float flash = flashLevel * exp(-d * d / (2.0 * 260.0 * 260.0));
            ring *= smoothstep(0.4, 2.0, t);                 // the wave starts after the flash
            glow += (0.30 * ring + 0.9 * flash) * flashGain;
        }
        else
        {
            float travel = saturate(flashAge / 20.0);       // just a faint wave, no flash
            glow += 0.10 * exp(-pow((d - travel * 2400.0) / 260.0, 2.0)) * (1.0 - travel);
        }
        // Add less light where the picture is already nearly white,
        // so bright spots don't just turn into flat white.
        color += gas * mask * nebula * glow * saturate(1.25 - Luma(color));

        // 4. Color shimmer: the blue glow drifts toward teal and back.
        if (effects & EFFECT_SHIMMER)
        {
            float wave = Clouds(img / 1400.0 + nebulaTime * 0.012) * 2.0 - 1.0;
            float s = (TimeNoise(nebulaTime / 18.0, 102u) * 2.0 - 1.0) * 0.8 + wave * 0.6;
            color += gas * mask * nebula * s * float3(-0.10, 0.10, -0.02);
        }

        // 5. Light rays from the central star, turning very slowly.
        if (effects & EFFECT_RAYS)
        {
            float2 v = img - focusImage;
            float angle = Angle(v) / TAU + 0.5;        // 0..1 around the star
            float beams = Noise(float2(angle * 40.0 + nebulaTime * 0.02, 0.5));
            beams = pow(beams, 4.0) * (0.5 + 0.5 * Noise(float2(angle * 25.0, nebulaTime / 9.0)));
            float fall = exp(-d / 1100.0) * smoothstep(80.0, 400.0, d);
            color += float3(0.55, 0.72, 1.0) * beams * fall * 0.10 * nebula * saturate(wide * 2.0);
        }

        // 6. Newborn stars: small warm glows that slowly swell and fade.
        if (effects & EFFECT_NEWSTARS)
        {
            [unroll] for (int i = 0; i < 8; i++)
            {
                float pulse = pow(TimeNoise(nebulaTime / (5.0 + i * 0.9), 200u + (uint)i), 5.0) * 1.6;
                float r = length(img - kNewStars[i]);
                float spot = exp(-r * r / (2.0 * 28.0 * 28.0)) + 0.25 * exp(-r * r / (2.0 * 90.0 * 90.0));
                color += float3(1.0, 0.86, 0.70) * spot * pulse * 0.35 * strength;
            }
        }
    }

    color += starLight.Load(int3(pos.xy, 0)).rgb;
    // A tiny bit of noise hides banding in the smooth glow.
    float dither = (Hash((uint)pos.x * 7919u + (uint)pos.y * 104729u) - 0.5) / 255.0;
    return float4(color * fade + dither, 1.0);
}
