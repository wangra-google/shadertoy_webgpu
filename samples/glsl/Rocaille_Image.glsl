// ROCAILLE — IMAGE
// ❤ Original @ https://www.shadertoy.com/view/WXyczK by XorDev ❤
// Interactive version
//
// Image iChannel0 -> Buffer A

const int PARAM_COUNT = 9;

const float PANEL_X   = 18.0;
const float PANEL_TOP = 18.0;
const float PANEL_W   = 258.0;
const float PANEL_H   = 270.0;
const float HEADER_H  = 28.0;
const float ROW_H     = 24.0;
const float TRACK_X0  = 91.0;
const float TRACK_X1  = 252.0;
const float PI        = 3.14159265359;
const float TAU       = 6.28318530718;

float getUIScale()
{
    // Must exactly match Buffer A.
    float heightScale = iResolution.y / 600.0;
    float widthScale  = iResolution.x / 720.0;
    return clamp(min(heightScale, widthScale), 1.0, 2.10);
}

float readControl(int id)
{
    return texelFetch(iChannel0, ivec2(id, 0), 0).r;
}

float sdRoundBox(vec2 p, vec2 halfSize, float radius)
{
    vec2 q = abs(p) - halfSize + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

vec3 controlColour(float id)
{
    float hue = id / float(PARAM_COUNT);
    return 0.58 + 0.42 * cos(TAU * (hue + vec3(0.00, 0.17, 0.34)));
}

// Compact embedded 4x6 uppercase bitmap font.
uint glyphBits(int character)
{
    if (character == 65) return 0x99f996u; // A
    if (character == 66) return 0x799797u; // B
    if (character == 67) return 0xe1111eu; // C
    if (character == 68) return 0x799997u; // D
    if (character == 69) return 0xf1171fu; // E
    if (character == 70) return 0x11171fu; // F
    if (character == 71) return 0xe9d11eu; // G
    if (character == 72) return 0x999f99u; // H
    if (character == 73) return 0xf6666fu; // I
    if (character == 76) return 0xf11111u; // L
    if (character == 77) return 0x999ff9u; // M
    if (character == 78) return 0x9ddbb9u; // N
    if (character == 79) return 0x699996u; // O
    if (character == 80) return 0x117997u; // P
    if (character == 81) return 0xed9996u; // Q
    if (character == 82) return 0x957997u; // R
    if (character == 83) return 0x78861eu; // S
    if (character == 84) return 0x66666fu; // T
    if (character == 85) return 0x699999u; // U
    if (character == 86) return 0x669999u; // V
    if (character == 87) return 0x9ff999u; // W
    if (character == 89) return 0x666699u; // Y
    if (character == 90) return 0xf1248fu; // Z
    return 0u;
}

float glyph4x6(vec2 fragCoord, vec2 origin, float scale, int character)
{
    vec2 cellPosition = (fragCoord - origin) / scale;
    ivec2 cell = ivec2(floor(cellPosition));

    if (cell.x < 0 || cell.x >= 4 || cell.y < 0 || cell.y >= 6)
    {
        return 0.0;
    }

    int sourceRow = 5 - cell.y;
    uint bitIndex = uint(sourceRow * 4 + cell.x);
    return float((glyphBits(character) >> bitIndex) & 1u);
}

ivec4 controlLabel(int id)
{
    if (id == 0) return ivec4(83, 67, 65, 76); // SCAL
    if (id == 1) return ivec4(76, 69, 78, 71); // LENG
    if (id == 2) return ivec4(68, 69, 84, 76); // DETL
    if (id == 3) return ivec4(84, 85, 82, 66); // TURB
    if (id == 4) return ivec4(70, 82, 69, 81); // FREQ
    if (id == 5) return ivec4(83, 80, 69, 68); // SPED
    if (id == 6) return ivec4(76, 65, 89, 82); // LAYR
    if (id == 7) return ivec4(72, 85, 69, 32); // HUE
    return ivec4(71, 76, 79, 87);              // GLOW
}

float wordMask(vec2 fragCoord, vec2 origin, float scale, ivec4 characters)
{
    float mask = 0.0;
    float advance = 5.0 * scale;

    for (int characterIndex = 0; characterIndex < 4; ++characterIndex)
    {
        mask = max(
            mask,
            glyph4x6(
                fragCoord,
                origin + vec2(float(characterIndex) * advance, 0.0),
                scale,
                characters[characterIndex]
            )
        );
    }

    return mask;
}

vec4 renderRocaille(vec2 fragCoord)
{
    float scaleControl      = readControl(0);
    float lengthControl     = readControl(1);
    float detailControl     = readControl(2);
    float turbulenceControl = readControl(3);
    float frequencyControl  = readControl(4);
    float speedControl      = readControl(5);
    float layerControl      = readControl(6);
    float hueControl        = readControl(7);
    float glowControl       = readControl(8);

    // At the default slider positions these values are exactly the supplied
    // source settings: .3, 9, 9, 1, 1, 1, 0 hue, and 1 glow.
    float spatialScale = mix(0.10, 0.70, scaleControl);
    float lengthPower  = mix(0.25, 2.00, lengthControl);
    int detail         = int(floor(mix(3.0, 30.0, detailControl) + 0.5));
    float turbulence   = mix(0.0, 2.0, turbulenceControl);
    float frequency    = mix(0.25, 2.50, frequencyControl);
    float speed        = mix(0.0, 3.0, speedControl);
    int layers         = int(floor(mix(1.0, 12.0, layerControl) + 0.5));
    float hueOffset    = mix(-PI, PI, hueControl);
    float glow         = mix(0.25, 2.00, glowControl);

    vec2 p =
        (2.0 * fragCoord - iResolution.xy) /
        iResolution.y /
        spatialScale;

    // Keep subtle pointer parallax outside the UI without altering the source
    // defaults when the pointer has not been moved.
    float uiScale = getUIScale();
    vec2 uiMouse = iMouse.xy / uiScale;
    vec2 uiResolution = iResolution.xy / uiScale;
    float panelTopY = uiResolution.y - PANEL_TOP;

    bool pointerOverPanel =
        uiMouse.x >= PANEL_X &&
        uiMouse.x <= PANEL_X + PANEL_W &&
        uiMouse.y <= panelTopY &&
        uiMouse.y >= panelTopY - PANEL_H;

    if (!pointerOverPanel && dot(iMouse.xy, iMouse.xy) > 0.0)
    {
        vec2 pointer =
            (2.0 * iMouse.xy - iResolution.xy) /
            iResolution.y;

        p += pointer * 0.10;
    }

    vec4 accumulated = vec4(0.0);

    // Explicit form of the compressed source loop. At defaults this evaluates
    // layers i = 1..9 and turbulence frequencies f = 1..9.
    for (int layerIndex = 1; layerIndex <= 12; ++layerIndex)
    {
        if (layerIndex > layers)
        {
            break;
        }

        float layer = float(layerIndex);
        vec2 turbulent = p;

        for (int iterationIndex = 1; iterationIndex <= 30; ++iterationIndex)
        {
            if (iterationIndex > detail)
            {
                break;
            }

            float iteration = float(iterationIndex);

            turbulent +=
                turbulence *
                sin(
                    turbulent.yx * (iteration * frequency) +
                    layer +
                    iTime * speed
                ) /
                iteration;
        }

        float attenuation = pow(max(length(turbulent), 1e-4), lengthPower);

        // Exact source palette at neutral hue:
        // (cos(i + vec4(0,1,2,3)) + 1.) / 6.
        vec4 layerColour =
            (cos(
                layer +
                vec4(0.0, 1.0, 2.0, 3.0) +
                hueOffset
            ) + 1.0) /
            6.0;

        accumulated += layerColour / attenuation;
    }

    accumulated *= glow;

    // Exact source tonemapping.
    vec4 colour = tanh(accumulated * accumulated);
    colour.a = 1.0;
    return colour;
}

void drawInterface(inout vec3 colour, vec2 pixelCoord)
{
    float uiScale = getUIScale();
    vec2 fragCoord = pixelCoord / uiScale;
    vec2 uiResolution = iResolution.xy / uiScale;

    float panelTopY = uiResolution.y - PANEL_TOP;
    vec2 panelCentre = vec2(
        PANEL_X + PANEL_W * 0.5,
        panelTopY - PANEL_H * 0.5
    );

    float panelDistance = sdRoundBox(
        fragCoord - panelCentre,
        vec2(PANEL_W, PANEL_H) * 0.5,
        10.0
    );

    float panelMask = 1.0 - smoothstep(0.0, 1.5, panelDistance);
    float borderMask = 1.0 - smoothstep(0.0, 1.2, abs(panelDistance) - 0.3);

    colour = mix(
        colour,
        vec3(0.010, 0.013, 0.022),
        panelMask * 0.88
    );

    colour += borderMask * vec3(0.12, 0.15, 0.22);

    // Header: CTRL
    float titleMask = wordMask(
        fragCoord,
        vec2(PANEL_X + 12.0, panelTopY - 20.0),
        2.0,
        ivec4(67, 84, 82, 76)
    );

    colour = mix(colour, vec3(0.88, 0.92, 1.0), titleMask);

    float headerLine = 1.0 - smoothstep(
        0.0,
        1.0,
        abs(fragCoord.y - (panelTopY - HEADER_H))
    );

    float headerWidth =
        step(PANEL_X + 8.0, fragCoord.x) *
        step(fragCoord.x, PANEL_X + PANEL_W - 8.0);

    colour += headerLine * headerWidth * vec3(0.07, 0.09, 0.14);

    for (int id = 0; id < PARAM_COUNT; ++id)
    {
        float value = readControl(id);
        float rowCentreY =
            panelTopY - HEADER_H -
            (float(id) + 0.5) * ROW_H;

        vec3 accent = controlColour(float(id));

        float rowMask =
            step(abs(fragCoord.y - rowCentreY), ROW_H * 0.5 - 1.0) *
            step(PANEL_X + 6.0, fragCoord.x) *
            step(fragCoord.x, PANEL_X + PANEL_W - 6.0);

        colour += rowMask * accent * (id % 2 == 0 ? 0.010 : 0.005);

        float labelMask = wordMask(
            fragCoord,
            vec2(PANEL_X + 12.0, rowCentreY - 6.0),
            1.7,
            controlLabel(id)
        );

        colour = mix(
            colour,
            mix(vec3(0.50), accent, 0.35),
            labelMask
        );

        float trackYDistance = abs(fragCoord.y - rowCentreY);
        float trackInsideX =
            step(TRACK_X0, fragCoord.x) *
            step(fragCoord.x, TRACK_X1);

        float trackMask =
            (1.0 - smoothstep(1.2, 2.5, trackYDistance)) *
            trackInsideX;

        colour = mix(colour, vec3(0.10, 0.12, 0.17), trackMask);

        float handleX = mix(TRACK_X0, TRACK_X1, value);
        float fillInsideX =
            step(TRACK_X0, fragCoord.x) *
            step(fragCoord.x, handleX);

        float fillMask =
            (1.0 - smoothstep(0.8, 2.0, trackYDistance)) *
            fillInsideX;

        colour = mix(colour, accent * 0.82, fillMask);

        float handleDistance = length(fragCoord - vec2(handleX, rowCentreY));
        float handleMask = 1.0 - smoothstep(4.2, 5.8, handleDistance);
        float handleCore = 1.0 - smoothstep(1.3, 3.0, handleDistance);

        colour = mix(colour, accent * 0.55, handleMask);
        colour = mix(colour, vec3(0.96), handleCore);
    }

    // Reset button.
    float resetTop =
        panelTopY - HEADER_H -
        float(PARAM_COUNT) * ROW_H - 3.0;

    vec2 resetCentre = vec2(PANEL_X + 209.0, resetTop - 9.5);

    float resetDistance = sdRoundBox(
        fragCoord - resetCentre,
        vec2(33.0, 9.5),
        4.0
    );

    float resetMask = 1.0 - smoothstep(0.0, 1.2, resetDistance);
    colour = mix(colour, vec3(0.10, 0.12, 0.18), resetMask);

    float resetText = wordMask(
        fragCoord,
        resetCentre + vec2(-21.0, -5.0),
        1.5,
        ivec4(82, 83, 84, 32)
    );

    colour = mix(colour, vec3(0.78, 0.82, 0.92), resetText);
}

void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec4 rocaille = renderRocaille(fragCoord);
    vec3 colour = rocaille.rgb;

    drawInterface(colour, fragCoord);

    fragColor = vec4(colour, 1.0);
}
