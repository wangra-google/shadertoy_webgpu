#ifndef CORE_CONFIG_H_
#define CORE_CONFIG_H_

#include <cstdint>

namespace config {
    // Native Desktop Window defaults
    constexpr uint32_t kNativeWindowWidth = 1280;
    constexpr uint32_t kNativeWindowHeight = 720;
    constexpr const char* kNativeWindowTitle = "Shadertoy WebGPU";

    // Web / WebAssembly Canvas defaults
    constexpr uint32_t kWebDefaultWidth = 800;
    constexpr uint32_t kWebDefaultHeight = 600;
    constexpr const char* kWebCanvasSelector = "#canvas";
}

#endif // CORE_CONFIG_H_

