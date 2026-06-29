#ifndef CORE_SHADERTOY_COMPAT_H_
#define CORE_SHADERTOY_COMPAT_H_

#include <string_view>

// GLSL header prepended to user-provided Shadertoy GLSL shaders.
// It maps the standard Shadertoy inputs and uniform structures to WebGPU-compatible locations.
inline constexpr std::string_view GLSL_SHADERTOY_HEADER = R"(#version 450
layout(location = 0) out vec4 outColor;

// Input attributes from the vertex shader.
// Note: Locations must match the vertex shader output layout.
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec2 v_uv;
layout(location = 2) in vec3 v_worldPos;

layout(set = 0, binding = 0) uniform ShadertoyUniforms {
    vec3 iResolution;
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    int iFrame;
    vec4 iMouse;
    vec4 iDate;
};

layout(set = 1, binding = 0) uniform sampler s_sampler;
layout(set = 1, binding = 1) uniform texture2D iChannel0;
layout(set = 1, binding = 2) uniform texture2D iChannel1;
layout(set = 1, binding = 3) uniform texture2D iChannel2;
layout(set = 1, binding = 4) uniform texture2D iChannel3;

// Macro mapping WebGL-style sampler2D access to Vulkan/WebGPU separate texture/sampler bindings,
// and automatically flipping the Y coordinate to match Shadertoy's Y-up texture space.
#define texture(tex, uv_coord) texture(sampler2D(tex, s_sampler), vec2((uv_coord).x, 1.0 - (uv_coord).y))
#define textureLod(tex, uv_coord, lod) textureLod(sampler2D(tex, s_sampler), vec2((uv_coord).x, 1.0 - (uv_coord).y), lod)

// User entry point declaration
void mainImage(out vec4 fragColor, in vec2 fragCoord);

void main() {
    // Shadertoy coordinates are 0-based pixels
    vec2 fragCoord = v_uv * iResolution.xy;
    vec4 fragColor;
    mainImage(fragColor, fragCoord);
    outColor = fragColor;
}
)";

#endif  // CORE_SHADERTOY_COMPAT_H_

