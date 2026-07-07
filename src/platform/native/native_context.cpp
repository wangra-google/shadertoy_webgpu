#include "platform/native/native_context.h"

#include <iostream>
#include <vector>

#include "core/config.h"

#if defined(_WIN32)
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include <windows.h>
#elif defined(__APPLE__)
    #define GLFW_EXPOSE_NATIVE_COCOA
#else
    #define GLFW_EXPOSE_NATIVE_X11
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>

NativeContext::NativeContext(uint32_t width, uint32_t height) : mWidth(width), mHeight(height) {}

NativeContext::~NativeContext() {
    if (mSurface) {
        mSurface.Unconfigure();
    }

    if (mWindow) {
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }
}

bool NativeContext::Init() {
    // Set Dawn's proc table
    dawnProcSetProcs(&dawn::native::GetProcs());

    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW!" << std::endl;
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    mWindow = glfwCreateWindow(mWidth, mHeight, config::kNativeWindowTitle, nullptr, nullptr);
    if (!mWindow) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(mWindow, this);

    // Create Instance requesting TimedWaitAny to support blocking waits on futures
    wgpu::InstanceDescriptor instanceDesc;
    static constexpr auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    instanceDesc.requiredFeatureCount = 1;
    instanceDesc.requiredFeatures = &kTimedWaitAny;
    mInstance = wgpu::CreateInstance(&instanceDesc);
    if (!mInstance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface depending on platform
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(mWindow);
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    wgpu::SurfaceSourceWindowsHWND winDesc;
    winDesc.hwnd = hwnd;
    winDesc.hinstance = hinstance;

    wgpu::SurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = &winDesc;
    mSurface = mInstance.CreateSurface(&surfaceDesc);
#elif defined(__APPLE__)
    wgpu::SurfaceSourceMetalLayer metalDesc;
    metalDesc.layer = nullptr; // Would be set to CAMetalLayer

    wgpu::SurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = &metalDesc;
    mSurface = mInstance.CreateSurface(&surfaceDesc);
#else
    wgpu::SurfaceSourceXlibWindow x11Desc;
    x11Desc.display = glfwGetX11Display();
    x11Desc.window = glfwGetX11Window(mWindow);

    wgpu::SurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = &x11Desc;
    mSurface = mInstance.CreateSurface(&surfaceDesc);
#endif

    if (!mSurface) {
        std::cerr << "Failed to create WGPU Surface!" << std::endl;
        return false;
    }

    // Request Adapter
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = mSurface;
    options.powerPreference = wgpu::PowerPreference::HighPerformance;

    wgpu::Adapter adapter = nullptr;
    wgpu::Future adapterFuture = mInstance.RequestAdapter(
        &options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapterIn, wgpu::StringView message) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                adapter = adapterIn;
            } else {
                std::cerr << "Request adapter failed: "
                          << (message.data ? std::string_view(message.data, message.length) : "Unknown error")
                          << std::endl;
            }
        });

    mInstance.WaitAny(adapterFuture, UINT64_MAX);

    mAdapter = adapter;
    if (!mAdapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Enumerate features to check for optional TimestampQuery
    std::vector<wgpu::FeatureName> requiredFeatures;
    wgpu::SupportedFeatures supportedFeatures;
    mAdapter.GetFeatures(&supportedFeatures);
    for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
        if (supportedFeatures.features[i] == wgpu::FeatureName::TimestampQuery) {
            requiredFeatures.push_back(wgpu::FeatureName::TimestampQuery);
            mTimestampsSupported = true;
            break;
        }
    }

    // Request Logical Device
    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.label = "Renderer Logical Device";
    deviceDesc.requiredFeatureCount = requiredFeatures.size();
    deviceDesc.requiredFeatures = requiredFeatures.data();

    // Setup uncaptured error callback
    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            std::cerr << "WebGPU Uncaptured Error (Type " << static_cast<int>(type) << "): "
                      << (message.data ? std::string_view(message.data, message.length) : "Unknown error")
                      << std::endl;
        });

    // Setup device lost callback
    deviceDesc.SetDeviceLostCallback(
        wgpu::CallbackMode::AllowSpontaneous,
        [](const wgpu::Device&, wgpu::DeviceLostReason reason, wgpu::StringView message) {
            std::cerr << "WebGPU Device Lost (Reason " << static_cast<int>(reason) << "): "
                      << (message.data ? std::string_view(message.data, message.length) : "Unknown reason")
                      << std::endl;
        });

    wgpu::Device device = nullptr;
    wgpu::Future deviceFuture = mAdapter.RequestDevice(
        &deviceDesc, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device deviceIn, wgpu::StringView message) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                device = deviceIn;
            } else {
                std::cerr << "Request device failed: "
                          << (message.data ? std::string_view(message.data, message.length) : "Unknown error")
                          << std::endl;
            }
        });

    mInstance.WaitAny(deviceFuture, UINT64_MAX);

    mDevice = device;
    if (!mDevice) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    mQueue = mDevice.GetQueue();

    // Initial Swapchain creation
    return CreateSwapChain(mWidth, mHeight);
}

bool NativeContext::CreateSwapChain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return false;
    }

    mWidth = width;
    mHeight = height;

    wgpu::SurfaceConfiguration config;
    config.device = mDevice;
    config.format = mSwapChainFormat;
    config.usage = wgpu::TextureUsage::RenderAttachment;
    config.width = mWidth;
    config.height = mHeight;
    config.presentMode = wgpu::PresentMode::Fifo;
    config.alphaMode = wgpu::CompositeAlphaMode::Auto;

    mSurface.Configure(&config);
    return true;
}

wgpu::TextureView NativeContext::GetCurrentSwapChainView() {
    wgpu::SurfaceTexture surfaceTexture = {};
    mSurface.GetCurrentTexture(&surfaceTexture);

    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << static_cast<int>(surfaceTexture.status)
                  << std::endl;
        return nullptr;
    }

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.label = "SwapChain View";
    viewDesc.format = mSwapChainFormat;
    viewDesc.dimension = wgpu::TextureViewDimension::e2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = wgpu::TextureAspect::All;

    wgpu::TextureView view = surfaceTexture.texture.CreateView(&viewDesc);

    return view;
}

void NativeContext::Present() {
    mSurface.Present();
}

bool NativeContext::ShouldClose() const {
    if (!mWindow) {
        return true;
    }
    return glfwWindowShouldClose(mWindow);
}

void NativeContext::PollEvents() {
    glfwPollEvents();
    if (mInstance) {
        mInstance.ProcessEvents();
    }
}
