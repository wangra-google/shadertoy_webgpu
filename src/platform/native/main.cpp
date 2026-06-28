#include <GLFW/glfw3.h>

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include "core/config.h"
#include "core/renderer.h"
#include "platform/native/native_context.h"

#include <glslang/Public/ShaderLang.h>
#include <tint/tint.h>

struct AppState {
    NativeContext* context;
    Renderer* renderer;
};

// GLFW resize callback
void OnFramebufferResize(GLFWwindow* window, int width, int height) {
    auto* appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (appState && appState->renderer) {
        appState->renderer->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    }
}

// GLFW key callback
void OnKey(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto* appState = static_cast<AppState*>(glfwGetWindowUserPointer(window));
    if (!appState || !appState->renderer) {
        return;
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) {
            std::cout << "Switching to Fullscreen Quad geometry" << std::endl;
            appState->renderer->SetGeometryType(GeometryType::Quad);
        } else if (key == GLFW_KEY_2) {
            std::cout << "Switching to Procedural Cube geometry" << std::endl;
            appState->renderer->SetGeometryType(GeometryType::Cube);
        } else if (key == GLFW_KEY_3) {
            std::cout << "Switching to Procedural Sphere geometry" << std::endl;
            appState->renderer->SetGeometryType(GeometryType::Sphere);
        } else if (key == GLFW_KEY_ESCAPE) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

int main(int argc, char* argv[]) {
    // Initialize shader compilers globally at application startup
    if (!glslang::InitializeProcess()) {
        std::cerr << "Failed to initialize glslang process!" << std::endl;
        return -1;
    }
    tint::Initialize();

    std::cout << "Starting Shadertoy WebGPU Native Desktop..." << std::endl;
    std::cout << "Keys for testing geometry pipeline:" << std::endl;
    std::cout << "  [1] Fullscreen Quad" << std::endl;
    std::cout << "  [2] Procedural Cube" << std::endl;
    std::cout << "  [3] Procedural Sphere" << std::endl;
    std::cout << "  [ESC] Exit" << std::endl;

    const uint32_t initialWidth = config::kNativeWindowWidth;
    const uint32_t initialHeight = config::kNativeWindowHeight;

    auto context = std::make_unique<NativeContext>(initialWidth, initialHeight);
    if (!context->Init()) {
        std::cerr << "Initialization of Native Context failed!" << std::endl;
        return -1;
    }

    auto renderer = std::make_unique<Renderer>(context.get());
    if (!renderer->Init()) {
        std::cerr << "Initialization of Renderer failed!" << std::endl;
        return -1;
    }

    AppState appState = {context.get(), renderer.get()};
    glfwSetWindowUserPointer(context->GetWindow(), &appState);
    glfwSetFramebufferSizeCallback(context->GetWindow(), OnFramebufferResize);
    glfwSetKeyCallback(context->GetWindow(), OnKey);

    // Timing tracking variables
    auto startTime = std::chrono::high_resolution_clock::now();
    auto lastFrameTime = startTime;
    int frameCount = 0;
    float fpsAccumulator = 0.0f;
    int fpsFrameCount = 0;
    float currentFps = 60.0f;

    // Render loop
    while (!context->ShouldClose()) {
        context->PollEvents();

        auto now = std::chrono::high_resolution_clock::now();
        float totalTime = std::chrono::duration<float>(now - startTime).count();
        float timeDelta = std::chrono::duration<float>(now - lastFrameTime).count();
        lastFrameTime = now;

        // Calculate simple frame rate averaging
        fpsAccumulator += timeDelta;
        fpsFrameCount++;
        if (fpsAccumulator >= 0.5f) {
            currentFps = static_cast<float>(fpsFrameCount) / fpsAccumulator;
            fpsAccumulator = 0.0f;
            fpsFrameCount = 0;
        }

        // Handle mouse inputs (dummy values for CLI context in Phase 1)
        double mouseX = 0.0, mouseY = 0.0;
        glfwGetCursorPos(context->GetWindow(), &mouseX, &mouseY);
        float mouseXF = static_cast<float>(mouseX);
        float mouseYF = static_cast<float>(mouseY);

        bool isMouseDown =
            glfwGetMouseButton(context->GetWindow(), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        float mouseZ = isMouseDown ? mouseXF : 0.0f;
        float mouseW = isMouseDown ? mouseYF : 0.0f;

        // Feed inputs to renderer uniforms
        renderer->SetUniforms(totalTime, timeDelta, currentFps, frameCount, mouseXF, mouseYF,
                              mouseZ, mouseW);

        // Execute render encoder commands
        renderer->Render();
        frameCount++;

        // Cap frame rate slightly to avoid consuming 100% CPU thread in idle loops
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    // Cleanup resources
    glfwSetWindowUserPointer(context->GetWindow(), nullptr);

    // Shutdown shader compilers globally at application exit
    tint::Shutdown();
    glslang::FinalizeProcess();

    std::cout << "Application exited successfully." << std::endl;
    return 0;
}
