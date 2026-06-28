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
