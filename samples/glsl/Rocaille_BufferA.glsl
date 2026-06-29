// ROCAILLE — BUFFER A
// ❤ Original @ https://www.shadertoy.com/view/WXyczK by XorDev ❤
// Added controls 
//
// SETUP
// 1. Buffer A iChannel0 -> Buffer A
// 2. Filtering: Nearest
// 3. Wrap: Clamp

const int PARAM_COUNT = 9;

const float PANEL_X   = 18.0;
const float PANEL_TOP = 18.0;
const float HEADER_H  = 28.0;
const float ROW_H     = 24.0;
const float TRACK_X0  = 91.0;
const float TRACK_X1  = 252.0;

float getUIScale()
{
    // Approximately 1.8x at 1080p; capped for 1440p and 4K.
    float heightScale = iResolution.y / 600.0;
    float widthScale  = iResolution.x / 720.0;
    return clamp(min(heightScale, widthScale), 1.0, 2.10);
}

float defaultControl(int id)
{
    // These defaults reproduce the supplied source:
    // scale .3, 9 layers, 9 turbulence steps, strength/frequency/speed 1,
    // no hue shift, no extra glow.
    if (id == 0) return 0.333333; // SCAL: 0.30 in range 0.10..0.70
    if (id == 1) return 0.428571; // LENG: exponent 1.0 in range 0.25..2.0
    if (id == 2) return 0.222222; // DETL: 9 steps in range 3..30
    if (id == 3) return 0.500000; // TURB: strength 1.0 in range 0..2
    if (id == 4) return 0.333333; // FREQ: frequency 1.0 in range .25..2.5
    if (id == 5) return 0.333333; // SPED: speed 1.0 in range 0..3
    if (id == 6) return 0.727273; // LAYR: 9 layers in range 1..12
    if (id == 7) return 0.500000; // HUE: neutral
    return 0.428571;              // GLOW: multiplier 1.0 in range .25..2
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    ivec2 pixel = ivec2(floor(fragCoord));

    // Only nine texels are used as persistent state.
    if (pixel.y != 0 || pixel.x < 0 || pixel.x >= PARAM_COUNT)
    {
        fragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    int id = pixel.x;
    float value = iFrame == 0
        ? defaultControl(id)
        : texelFetch(iChannel0, ivec2(id, 0), 0).r;

    float uiScale = getUIScale();
    vec2 uiResolution = iResolution.xy / uiScale;

    // iMouse.zw is where the current press started.
    // iMouse.z > 0 while mouse, touchpad click, or touch remains held.
    if (iMouse.z > 0.0)
    {
        float pressX   = iMouse.z / uiScale;
        float pressY   = iMouse.w / uiScale;
        float currentX = iMouse.x / uiScale;

        float firstRowTop = uiResolution.y - PANEL_TOP - HEADER_H;
        int activeRow = int(floor((firstRowTop - pressY) / ROW_H));

        bool startedOnTrack =
            pressX >= TRACK_X0 - 12.0 &&
            pressX <= TRACK_X1 + 12.0 &&
            activeRow >= 0 &&
            activeRow < PARAM_COUNT;

        if (startedOnTrack && activeRow == id)
        {
            value = clamp(
                (currentX - TRACK_X0) / (TRACK_X1 - TRACK_X0),
                0.0,
                1.0
            );
        }

        float resetTop =
            uiResolution.y - PANEL_TOP - HEADER_H -
            float(PARAM_COUNT) * ROW_H - 3.0;

        float resetBottom = resetTop - 19.0;

        bool resetPressed =
            pressX >= PANEL_X + 176.0 &&
            pressX <= PANEL_X + 242.0 &&
            pressY >= resetBottom &&
            pressY <= resetTop;

        if (resetPressed)
        {
            value = defaultControl(id);
        }
    }

    fragColor = vec4(value, 0.0, 0.0, 1.0);
}
