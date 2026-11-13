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
    float  swirlTime;     // clock for the swirling center (runs faster/slower with "Swirl speed")
    float3 padding2;
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

// x squared (safe for negative x, unlike pow on some graphics cards).
float Sq(float x) { return x * x; }

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

// Like RandomEvent, but returns how many seconds ago the event started
// (or -1 if none is going on). Used for star bursts, which have several stages.
float EventAge(uint seed, float slotLength, float chance, float duration)
{
    uint k = (uint)floor(realTime / slotLength);
    float age = -1.0;
    for (uint j = 0u; j < 2u; j++)
    {
        if (k < j)
            break;
        uint kk = k - j;
        uint h = kk * 2654435761u ^ seed * 40503u;
        if (Hash(h) < chance)
        {
            float a = realTime - (kk + Hash(h + 977u)) * slotLength;
            if (a >= 0.0 && a < duration)
                age = a;
        }
    }
    return age;
}

// Star burst timeline (seconds after it starts):
//   0.0 - 1.0   build-up: the star brightens and flickers faster and faster
//   1.0 - 1.12  a short dip, like a breath in
//   1.12        pop: big glow and four spikes shooting out
//   to 2.4      afterglow fades back to the normal star
static const float kBurstLength = 2.4;
static const float kPopAt = 1.12;

struct StarVertex
{
    float4 pos       : SV_Position;
    float2 center    : CENTER;     // star center on screen
    float  coreSize  : CORE;       // twinkle area radius on screen
    float  quadSize  : QUAD;       // drawn square half-size on screen
    float  amount    : AMOUNT;     // brighten/dim the star itself: -1..+1 or more
    float  glint     : GLINT;      // spike flash of big stars: 0..1
    float2 burst     : BURST;      // pop glow, spike length (px)
    float4 tint      : TINT;       // color of the burst light; w = how big it is
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

    // Star bursts: rare, at random stars and random moments.
    // About one every 8-10 seconds on a 1080p screen at the normal setting.
    float2 burst = 0.0;
    float size = 0.0;
    float age = -1.0;
    if (burstRate > 0.0 && burstGain > 0.0 && brightness > 0.7)
        age = EventAge(starId * 5u + 2u, 8.0, min(0.0005 * burstRate, 1.0), kBurstLength);
    if (age >= 0.0)
    {
        size = 0.8 + 0.4 * sqrt(burstGain);
        amount = 0.0;   // no twinkling during a burst, so the build-up is clear
        if (age < 1.0)
        {
            // Build-up: brighter, with a flicker that speeds up.
            float b = age * sqrt(age);                  // starts gently, speeds up
            float flicker = TimeNoise(realTime * lerp(3.0, 16.0, age), starId + 5u) * 2.0 - 1.0;
            amount = (1.3 * b + 0.35 * b * flicker) * burstGain;
            burst.x = 0.45 * b * burstGain;             // a glow growing around it
        }
        else if (age < kPopAt)
        {
            amount = -0.4 * burstGain;                  // the breath in
        }
        else
        {
            float p = age - kPopAt;                     // seconds since the pop
            float pop = smoothstep(0.0, 0.04, p);
            amount = (2.5 * exp(-p / 0.35)) * pop * burstGain;
            burst.x = 1.6 * exp(-p / 0.22) * pop * burstGain;            // glow
            burst.y = (14.0 + 70.0 * saturate(p / 0.15)) * exp(-p / 0.45) * size; // spikes shoot out, then shrink
        }
    }

    float core = max((radius * 2.5 + 2.0) * scale, 1.5);
    float quad = canGlint ? max(radius * 10.0 * scale, core) : core;
    if (age >= 0.0)
        quad = max(quad, 90.0 * size);

    float2 center = focusScreen + (starPos - focusImage) * scale;
    float2 corner = float2((vertexId == 1 || vertexId == 2 || vertexId == 4) ? 1.0 : -1.0,
                           (vertexId == 2 || vertexId == 4 || vertexId == 5) ? 1.0 : -1.0);
    float2 screen = center + corner * (quad + 1.0);

    // Burst light takes the star's own color, mixed with white.
    float3 c = SamplePicture(starPos);
    float3 tint = lerp(c / max(max(c.r, max(c.g, c.b)), 0.05), 1.0, 0.55);

    StarVertex o;
    o.pos = float4(screen / rtSize * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.center = center;
    o.coreSize = core;
    o.quadSize = quad;
    o.amount = amount;
    o.glint = glint;
    o.burst = burst;
    o.tint = float4(tint, size);
    return o;
}

float4 StarPS(StarVertex v) : SV_Target
{
    float2 screen = v.pos.xy;
    float2 offset = screen - v.center;
    float dist = length(offset);
    float2 img = ScreenToImage(screen);

    float3 bg = SampleBackground(img);
    // Only the star's own light, not the gas behind it.
    // (A small extra margin keeps faint gas detail out of it.)
    float3 starOnly = max(SamplePicture(img) - bg * 1.05 - 0.03, 0.0);
    // Stars inside the bright nebula twinkle less, so the gas never flickers.
    float calm = 1.0 - smoothstep(0.30, 0.60, Luma(bg));

    float spikeFade = saturate(1.0 - dist / v.quadSize);
    // Area that brightens/dims with the twinkle: the whole star, evenly, fading
    // only at the very edge (for stars with spikes, that includes the spikes).
    float starArea = 1.0 - smoothstep(0.55, 1.0, dist / v.coreSize);
    // Stars with spikes: also twinkle the spikes, using a thin cross shape
    // (not a big circle, which would light up the stars next to it too).
    if (v.quadSize > v.coreSize * 1.5)
    {
        float2 r = float2(offset.x * 0.9994 - offset.y * 0.035, offset.x * 0.035 + offset.y * 0.9994);
        float cross_ = exp(-Sq(min(abs(r.x), abs(r.y))) / 2.0) * saturate(1.0 - dist / v.quadSize);
        starArea = max(starArea, cross_);
    }

    // Dimming is gentle: never more than 40% of the star's own light, so a
    // dimming star never stands out darker than its surroundings.
    float gain = max(v.amount, -0.4) * starArea + v.glint * 1.4 * spikeFade;
    float3 light = starOnly * gain * calm;

    // Extra light drawn for a burst (glow and four spikes).
    if (v.tint.w > 0.0)
    {
        float s = v.tint.w;
        float glow = exp(-dist * dist / (2.0 * 25.0 * s * s)) + 0.35 * exp(-dist * dist / (2.0 * 300.0 * s * s));
        // Spikes line up with the picture's own star spikes (nearly straight up/down and sideways).
        float2 d = float2(offset.x * 0.9994 - offset.y * 0.035, offset.x * 0.035 + offset.y * 0.9994);
        float spikeLen = max(v.burst.y, 1.0);
        float spikes = exp(-d.y * d.y / 1.2) * exp(-abs(d.x) / spikeLen)
                     + exp(-d.x * d.x / 1.2) * exp(-abs(d.y) / spikeLen);
        float edge = saturate(1.0 - dist / v.quadSize);   // fade out before the square's edge
        light += v.tint.rgb * (v.burst.x * glow + 0.8 * v.burst.x * spikes) * edge
                 * (0.4 + 0.6 * calm);
    }
    return float4(light, 0.0);
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
#define EFFECT_NEWSTARS 32u  // a few spots inside the gas softly pulse
// The "storm" effects.
#define EFFECT_LIGHTNING  64u   // split-second flashes deep inside the gas
#define EFFECT_CELLS      128u  // brighter, busier storm areas that wander around
#define EFFECT_CRACKLE    1024u // tiny bright threads flicker along the edges of the gas
#define EFFECT_SWIRL      2048u // the gas around the center slowly turns like a whirlpool
// Experiments (no checkbox yet; switched on with IRIS_DEMO_EFFECTS for demo videos).
#define EFFECT_RELIGHT    4096u  // the light inside slowly changes direction, shading the gas in 3D
#define EFFECT_MOTES      8192u  // tiny dust specks drift in front, at two depths
#define EFFECT_STREAMS    16384u // thin threads of light spiral in toward the central star
#define EFFECT_SWAY       32768u // the bright gas sways a little against the stars (depth)

// Smooth random "noise" (values 0..1 that change gradually across space).
// Random value 0..1 for a grid cell. Uses whole numbers, so it stays clean
// however far the patterns have moved (an older version broke down into
// straight stripes after the screensaver had run for a while).
float Hash2(float2 cell)
{
    int2 c = int2(floor(cell));
    return Hash((uint)c.x * 1597334677u ^ (uint)c.y * 3812015801u ^ 0x9E3779B9u);
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

// Smooth random pattern without any visible grid: "gradient noise", with each
// layer turned at a different angle so no straight lines or squares line up.
float2 Gradient(float2 cell)
{
    float a = Hash2(cell) * TAU;
    return float2(cos(a), sin(a));
}

float GradNoise(float2 p)
{
    float2 i = floor(p), f = frac(p);
    float2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    float a = dot(Gradient(i), f);
    float b = dot(Gradient(i + float2(1, 0)), f - float2(1, 0));
    float c = dot(Gradient(i + float2(0, 1)), f - float2(0, 1));
    float d = dot(Gradient(i + float2(1, 1)), f - float2(1, 1));
    return 0.5 + 0.7 * lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);   // about 0..1
}

float Clouds(float2 p)
{
    // Bend the space a little first so shapes look like wisps, not blobs.
    p += 0.35 * float2(GradNoise(p * 0.6 + 7.3), GradNoise(p * 0.6 + 21.9));
    const float2x2 turn = float2x2(0.80, -0.60, 0.60, 0.80);   // about 37 degrees
    float v = GradNoise(p) * 0.5;
    p = mul(turn, p) * 2.03 + 17.1;
    v += GradNoise(p) * 0.3;
    p = mul(turn, p) * 2.01 + 41.7;
    v += GradNoise(p) * 0.2;
    return saturate(v);
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

// ---- storm helpers

static const int kStormCells = 4;

// Where storm cell i is right now (picture pixels). Cells wander slowly and
// randomly within about 1500 pixels of the nebula's center.
float2 StormCellCenter(int i, float t)
{
    uint seed = 300u + (uint)i * 7u;
    float2 wander = float2(TimeNoise(t / (31.0 + i * 5.0), seed), TimeNoise(t / (37.0 + i * 3.0), seed + 1u));
    return focusImage + (wander - 0.5) * 3000.0;
}

// How "stormy" a spot is: 0 = calm, 1 = inside a storm cell.
float StormActivity(float2 img, float t)
{
    float a = 0.0;
    [unroll] for (int i = 0; i < kStormCells; i++)
    {
        float r = length(img - StormCellCenter(i, t));
        a = max(a, exp(-r * r / (2.0 * 520.0 * 520.0)));
    }
    return a;
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
        float wide = smoothstep(0.02, 0.12, Luma(gas)) * saturate(1.0 - d / 3800.0);
        float strength = saturate(nebula);   // 1 for Subtle and Stronger; Stronger also scales below

        // Storm cells decide where the storm effects are busiest.
        // With cells switched off, the storm is spread evenly.
        bool cells = (effects & EFFECT_CELLS) != 0;
        float storm = cells ? StormActivity(img, nebulaTime) : 0.45;

        // 1. Drifting gas. Two copies of the picture slide along the flow and
        //    take turns, so the gas keeps moving without ever tearing.
        if ((effects & EFFECT_DRIFT) && wide > 0.0)
        {
            const float period = 16.0;
            float2 flow = Flow(img) * 0.45 * nebula;          // picture pixels per second
            // Each area has its own timing so the whole nebula never "pulses" together.
            float offset = GradNoise(img / 1800.0 + 3.7);
            float ph0 = frac(nebulaTime / period + offset), ph1 = frac(nebulaTime / period + offset + 0.5);
            float3 a = SamplePicture(img - flow * ph0 * period);
            float3 b = SamplePicture(img - flow * ph1 * period);
            float3 moved = lerp(a, b, abs(ph0 - 0.5) * 2.0);
            // Only the gas moves: stars stay sharp and still.
            float keep = max(Starness(color, gas), Starness(moved, gas));
            color = lerp(color, moved, wide * (1.0 - keep));
        }

        // Experiment: depth sway. The bright gas shifts a few pixels back and
        // forth against the stars and the dark dust, as if seen in 3D.
        if (effects & EFFECT_SWAY)
        {
            float2 sway = (float2(TimeNoise(nebulaTime / 9.0, 601u), TimeNoise(nebulaTime / 11.0, 602u)) - 0.5) * 22.0;
            float layer = mask * strength;                    // brightest gas moves most
            float3 moved = SamplePicture(img - sway * layer);
            float keep = max(Starness(color, gas), Starness(moved, gas));
            color = lerp(color, moved, saturate(layer) * (1.0 - keep));
        }

        // 3. Passing dust shadows (in front of everything, so stars dim too).
        if (effects & EFFECT_SHADOWS)
        {
            float2 p = img / 950.0 + float2(nebulaTime * 0.010, nebulaTime * -0.006);
            float shade = smoothstep(0.52, 0.85, Clouds(p));
            color *= 1.0 - shade * 0.22 * nebula * wide;
        }

        // Gentle breathing glow (always on).
        float breath = TimeNoise(nebulaTime / 14.0, 101u);
        float glow = 0.06 * breath;
        float flashLight = 0.0;

        // 2. Nebula flash, built like the star bursts (seconds after it starts):
        //    0 - 1.2      build-up: the center brightens, flickering faster and faster
        //    1.2 - 1.32   a short dip, like a breath in
        //    1.32         pop: bright flash, gone again within a second
        //    after that   a soft wave of light spreads outward through the gas
        if (effects & EFFECT_ECHO)
        {
            const float popAt = 1.32;
            float t = flashAge;
            float center = exp(-d * d / (2.0 * 260.0 * 260.0));
            float level = 0.0;
            if (t < 1.2)
            {
                float b = t * sqrt(t) / 1.31;                // 0..1, speeding up
                float flicker = TimeNoise(realTime * lerp(3.0, 14.0, t / 1.2), 77u) * 2.0 - 1.0;
                level = 0.30 * b + 0.08 * b * flicker;
            }
            else if (t < popAt)
            {
                level = -0.10;
            }
            else
            {
                float p = t - popAt;
                float pop = smoothstep(0.0, 0.04, p);
                level = pop * (1.0 * exp(-p / 0.22) + 0.12 * exp(-p / 1.0));
            }
            // The flash lights the cloud from inside: brighter on the thick
            // strands of gas, darker where (made-up) clouds sit in front of it.
            // Every flash gets its own pattern.
            float start = realTime - t;
            float2 seed = float2(frac(start * 0.1234), frac(start * 0.3717)) * 50.0;
            float strands = saturate(Luma(color) / max(Luma(gas), 0.02) - 0.55);
            float front = smoothstep(0.42, 0.70, Clouds(img / 380.0 + seed));
            float hot = smoothstep(0.50, 0.80, Clouds(img / 260.0 + seed.yx + 7.0));
            float lit = (0.15 + 0.85 * strands + 0.8 * hot) * (1.0 - 0.90 * front);
            flashLight = level * center * lit * flashGain;

            // The wave of light, starting at the pop.
            float since = max(t - popAt, 0.0);
            float travel = saturate(since / 12.0);
            float ring = exp(-Sq((d - travel * 2600.0) / 320.0)) * (1.0 - travel) * step(popAt, t);
            ring *= smoothstep(0.2, 1.2, since);
            glow += 0.22 * ring * flashGain;
        }
        else
        {
            float travel = saturate(flashAge / 20.0);       // just a faint wave, no flash
            glow += 0.10 * exp(-Sq((d - travel * 2400.0) / 260.0)) * (1.0 - travel);
        }
        // Add less light where the picture is already nearly white,
        // so bright spots don't just turn into flat white.
        color += gas * mask * nebula * glow * saturate(1.25 - Luma(color));
        // The nebula flash, lighting both the gas glow and the detail in it.
        color += (color * 0.8 + gas * 0.9) * flashLight * wide * nebula * saturate(1.3 - Luma(color));

        // 4. Color shimmer: the blue glow drifts toward teal and back (never purple).
        if (effects & EFFECT_SHIMMER)
        {
            float wave = Clouds(img / 1400.0 + nebulaTime * 0.012) * 2.0 - 1.0;
            float s = (TimeNoise(nebulaTime / 18.0, 102u) * 2.0 - 1.0) * 0.8 + wave * 0.6;
            // Only toward teal: going the other way turns the gas purple.
            // At full shift: red -8%, green +6%, blue +3% of the gas's own color.
            color += gas * mask * nebula * saturate(s) * float3(-0.08, 0.06, 0.03);
        }

        // Storm cells glow a little and flicker softly.
        if (cells)
        {
            float flicker = TimeNoise(realTime * 1.7, 401u) * 0.6 + 0.4;
            color += gas * wide * storm * 0.10 * flicker * strength;
        }

        // Storm: lightning. Now and then a spot deep in the gas lights up for a
        // split second, often two or three times in a row, then it goes quiet.
        if (effects & EFFECT_LIGHTNING)
        {
            const float slot = 0.45;                            // seconds
            uint k = (uint)floor(realTime / slot);
            float light = 0.0;
            for (uint j = 0u; j < 2u; j++)
            {
                if (k < j)
                    break;
                uint kk = k - j;
                // Busy and quiet spells come and go.
                float busy = smoothstep(0.45, 0.80, TimeNoise(kk * slot / 7.0, 501u));
                uint h = kk * 2654435761u + 9091u;
                if (Hash(h) < 0.55 * busy)
                {
                    float age = realTime - (kk + Hash(h + 1u) * 0.6) * slot;
                    if (age >= 0.0)
                    {
                        // Where: inside a storm cell (or anywhere in the nebula).
                        int cell = (int)(Hash(h + 2u) * kStormCells) % kStormCells;
                        float2 jitter = (float2(Hash(h + 3u), Hash(h + 4u)) - 0.5) * (cells ? 700.0 : 2600.0);
                        float2 at = (cells ? StormCellCenter(cell, nebulaTime) : focusImage) + jitter;
                        float r = length(img - at);
                        float size = lerp(180.0, 380.0, Hash(h + 5u));
                        float spot = exp(-r * r / (2.0 * size * size));
                        // Flash shape: a sharp flash and sometimes a quick second one.
                        float f = exp(-age / 0.07);
                        if (Hash(h + 6u) < 0.5)
                            f += 0.7 * exp(-max(age - 0.11, 0.0) / 0.06) * step(0.11, age);
                        light = max(light, spot * f);
                    }
                }
            }
            // The light follows the gas itself, so the cloud seems lit from inside.
            float3 lit = color * 0.9 + gas * 1.2 + float3(0.05, 0.07, 0.10);
            color += lit * light * wide * 0.85 * strength * saturate(1.3 - Luma(color));
        }

        // Storm: crackle. Thin bright threads flash along the edges of the gas.
        if (effects & EFFECT_CRACKLE)
        {
            // Edges: where the gas brightness changes quickly.
            float e = 40.0;
            float gx = Luma(SampleBackground(img + float2(e, 0))) - Luma(SampleBackground(img - float2(e, 0)));
            float gy = Luma(SampleBackground(img + float2(0, e))) - Luma(SampleBackground(img - float2(0, e)));
            float edge = saturate(length(float2(gx, gy)) * 9.0);
            // Small, thin, jagged sparks that jump to new places many times a second.
            float step_ = floor(realTime * 10.0);
            float2 q = img / 16.0 + step_ * 13.7;
            float r1 = 1.0 - abs(GradNoise(q) * 2.0 - 1.0);
            float r2 = 1.0 - abs(GradNoise(q * 2.3 + 5.1) * 2.0 - 1.0);
            float spark = pow(saturate(r1 * r2), 16.0);
            // Only a few tiny patches crackle at any moment.
            float patch_ = smoothstep(0.80, 0.92, GradNoise(img / 140.0 + step_ * 5.3));
            color += float3(0.75, 0.88, 1.0) * spark * patch_ * edge * wide * (0.3 + storm) * 0.9 * strength;
        }

        // Swirling center: a small whirlpool of gas inside the bright core,
        // seen from the side (a tilted disk, top tipped away, seen slightly from
        // above), turning counterclockwise. It doesn't paint new clouds on top:
        // it bends and re-lights the picture's own gas, and dark dust in front
        // of the core stays in front. Size: about the red circle Keegan drew
        // (radius ~170 picture pixels around the central star).
        if ((effects & EFFECT_SWIRL) && d < 320.0)
        {
            const float R = 175.0;          // disk radius (picture pixels)
            const float tilt = 0.50;        // how flat the disk looks (1 = face on)
            float2 v = img - focusImage;
            // Perspective: the near (bottom) side looks a bit bigger than the far side.
            float persp = 1.0 + 0.22 * clamp(v.y / (R * tilt), -1.0, 1.0);
            float field = 0.0, weightSum = 0.0;
            float2 push = 0.0;
            [unroll] for (int layer = 0; layer < 3; layer++)
            {
                float k = layer / 2.0;                       // 0 = lowest layer, 1 = highest
                // Higher layers sit a little higher on screen (we look slightly down).
                float2 w = float2(v.x, v.y + (k - 0.5) * 0.18 * R) / persp;
                float2 disk = float2(w.x, w.y / tilt);       // flat disk coordinates
                float r = length(disk);
                // Counterclockwise on screen; each layer at its own speed, with a
                // fixed spiral twist, turning as a whole so it never winds up.
                float a = swirlTime * lerp(0.020, 0.032, k) + layer * 2.1 - 1.4 * log(max(r, 8.0) / 40.0);
                float2 q = float2(disk.x * cos(a) - disk.y * sin(a), disk.x * sin(a) + disk.y * cos(a));
                float size = lerp(34.0, 52.0, k);
                float n = Clouds(q / size + float2(layer * 9.3, layer * 4.1)) - 0.5;
                float inDisk = 1.0 - smoothstep(0.25, 1.0, r / R);
                inDisk *= inDisk;
                field += n * inDisk * lerp(0.7, 1.0, k);
                weightSum += inDisk;
                // Gas is carried around: push the picture along the turning direction.
                float2 along = normalize(float2(-disk.y, disk.x) + 0.001);
                push += along * n * inDisk * 8.0;
            }
            float soft = saturate(weightSum / 3.0);
            // Dark dust in front of the core: where the picture is darker than
            // the gas glow around it, the swirl is hidden behind it.
            float inFront = saturate((Luma(gas) - Luma(color)) * 5.0);
            float showing = soft * (1.0 - 0.85 * inFront) * strength * (1.0 - Starness(color, gas));
            // Far side (top) a little dimmer than the near side.
            float nearSide = 0.85 + 0.15 * clamp(v.y / (R * tilt), -1.0, 1.0);
            float3 bent = SamplePicture(img + float2(push.x, push.y * tilt) * showing);
            // Thicker gas glows brighter; the gaps between the wisps are darker and
            // a deeper blue, which is what makes the swirl show inside the white core.
            float gap = saturate(-field * 2.2);
            float3 swirled = bent * (1.0 + 0.45 * max(field, 0.0) * nearSide) + gas * max(field, 0.0) * 0.30;
            swirled = lerp(swirled, swirled * float3(0.62, 0.78, 1.0), gap * 0.75);
            color = lerp(color, swirled, showing);
        }

        // Experiment: moving inner light. The gas is treated like a landscape
        // (bright = high) and lit from a direction that slowly wanders around.
        if (effects & EFFECT_RELIGHT)
        {
            float e = 70.0;
            float gx = Luma(SampleBackground(img + float2(e, 0))) - Luma(SampleBackground(img - float2(e, 0)));
            float gy = Luma(SampleBackground(img + float2(0, e))) - Luma(SampleBackground(img - float2(0, e)));
            float3 n = normalize(float3(-gx * 9.0, -gy * 9.0, 1.0));
            float a = TimeNoise(nebulaTime / 25.0, 611u) * TAU * 2.0;
            float3 l = normalize(float3(cos(a), sin(a), 0.7));
            float shade = dot(n, l) - l.z;                   // >0 facing the light, <0 turned away
            color *= 1.0 + clamp(shade * 1.6, -0.18, 0.35) * wide * strength;
        }

        // Experiment: falling streams. Thin threads of light on inward spirals,
        // broken up so they look like wisps rather than a pattern.
        if (effects & EFFECT_STREAMS)
        {
            float2 v = img - focusImage;
            float angle = Angle(v) / TAU;                                  // -0.5 .. 0.5
            float r = max(d, 1.0);
            float x = angle * 3.0 + log(r) * 1.8 + nebulaTime * 0.04;      // 3 arms, moving inward
            float wave = 1.0 - abs(frac(x) * 2.0 - 1.0);
            float thread = pow(wave, 40.0);
            float pieces = smoothstep(0.55, 0.80, GradNoise(img / 420.0 + nebulaTime * 0.01));
            float band = smoothstep(150.0, 400.0, d) * (1.0 - smoothstep(1200.0, 1900.0, d));
            color += float3(0.70, 0.85, 1.0) * thread * pieces * band * gas * 1.2 * wide * strength;
        }

        // Experiment: floating dust. Tiny specks drifting at two depths.
        if (effects & EFFECT_MOTES)
        {
            [unroll] for (int layer = 0; layer < 2; layer++)
            {
                float size = layer == 0 ? 45.0 : 80.0;                       // picture pixels per cell
                float2 drift = (layer == 0 ? float2(3.0, 1.2) : float2(6.5, 2.4)) * nebulaTime;
                float2 p = (img + drift) / size;
                float2 cellId = floor(p);
                uint h = (uint)(cellId.x + 4096.0) * 73856093u ^ (uint)(cellId.y + 4096.0) * 19349663u ^ (uint)layer * 83492791u;
                if (Hash(h) < 0.14)
                {
                    float2 at = cellId + float2(Hash(h + 1u), Hash(h + 2u));
                    float dist = length(p - at) * size * scale;            // screen pixels
                    float dot_ = exp(-dist * dist / (layer == 0 ? 1.2 : 2.6));
                    float twinkle_ = 0.5 + 0.5 * TimeNoise(realTime * 0.7, h);
                    color += float3(0.85, 0.80, 0.75) * dot_ * twinkle_ * (layer == 0 ? 0.35 : 0.55) * strength * saturate(wide * 3.0 + 0.3);
                }
            }
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
