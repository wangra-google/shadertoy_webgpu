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
        wgpuSurfaceUnconfigure(mSurface);
        wgpuSurfaceRelease(mSurface);
    }
    if (mQueue) {
        wgpuQueueRelease(mQueue);
    }
    if (mDevice) {
        wgpuDeviceRelease(mDevice);
    }
    if (mAdapter) {
        wgpuAdapterRelease(mAdapter);
    }
    if (mInstance) {
        wgpuInstanceRelease(mInstance);
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

    // Set user pointer to context for callbacks
    glfwSetWindowUserPointer(mWindow, this);

    // Create Instance
    WGPUInstanceDescriptor instanceDesc = {};
    mInstance = wgpuCreateInstance(&instanceDesc);
    if (!mInstance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface depending on platform
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(mWindow);
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    WGPUSurfaceSourceWindowsHWND winDesc = {};
    winDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    winDesc.chain.next = nullptr;
    winDesc.hwnd = hwnd;
    winDesc.hinstance = hinstance;

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &winDesc.chain;
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
#elif defined(__APPLE__)
    // macOS Cocoa/Metal surface creation (stubbed/schematic for compilation)
    // In a full macOS build, we would configure a CAMetalLayer on the NSWindow.
    WGPUSurfaceSourceMetalLayer metalDesc = {};
    metalDesc.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    metalDesc.chain.next = nullptr;
    metalDesc.layer = nullptr; // Would be set to CAMetalLayer

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &metalDesc.chain;
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
#else
    // Linux X11 surface creation (stubbed/schematic for compilation)
    WGPUSurfaceSourceXlibWindow x11Desc = {};
    x11Desc.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
    x11Desc.chain.next = nullptr;
    x11Desc.display = glfwGetX11Display();
    x11Desc.window = glfwGetX11Window(mWindow);

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &x11Desc.chain;
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
#endif

    if (!mSurface) {
        std::cerr << "Failed to create WGPU Surface!" << std::endl;
        return false;
    }

    // Request Adapter
    WGPURequestAdapterOptions options = {};
    options.compatibleSurface = mSurface;
    options.powerPreference = WGPUPowerPreference_HighPerformance;

    struct AdapterRequestResult {
        WGPUAdapter adapter = nullptr;
        bool completed = false;
    } adapterResult;

    WGPURequestAdapterCallbackInfo adapterCallbackInfo = {};
    adapterCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    adapterCallbackInfo.callback = [](WGPURequestAdapterStatus status, WGPUAdapter adapter,
                                      WGPUStringView message, void* userdata1, void* userdata2) {
        auto* result = static_cast<AdapterRequestResult*>(userdata1);
        if (status == WGPURequestAdapterStatus_Success) {
            result->adapter = adapter;
        } else {
            std::cerr << "Request adapter failed: ";
            if (message.data && message.length > 0) {
                std::cerr.write(message.data, message.length);
            } else {
                std::cerr << "Unknown error";
            }
            std::cerr << std::endl;
        }
        result->completed = true;
    };
    adapterCallbackInfo.userdata1 = &adapterResult;

    WGPUFuture adapterFuture =
        wgpuInstanceRequestAdapter(mInstance, &options, adapterCallbackInfo);

    WGPUFutureWaitInfo adapterWaitInfo = {};
    adapterWaitInfo.future = adapterFuture;
    adapterWaitInfo.completed = false;

    while (!adapterResult.completed) {
        wgpuInstanceWaitAny(mInstance, 1, &adapterWaitInfo, UINT64_MAX);
    }

    mAdapter = adapterResult.adapter;
    if (!mAdapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Enumerate features to check for optional TimestampQuery
    std::vector<WGPUFeatureName> requiredFeatures;
    WGPUSupportedFeatures supportedFeatures = {};
    wgpuAdapterGetFeatures(mAdapter, &supportedFeatures);
    for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
        if (supportedFeatures.features[i] == WGPUFeatureName_TimestampQuery) {
            requiredFeatures.push_back(WGPUFeatureName_TimestampQuery);
            mTimestampsSupported = true;
            break;
        }
    }

    // Request Logical Device
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = {"Renderer Logical Device", WGPU_STRLEN};
    deviceDesc.requiredFeatureCount = requiredFeatures.size();
    deviceDesc.requiredFeatures = requiredFeatures.data();

    deviceDesc.uncapturedErrorCallbackInfo.callback = [](WGPUDevice const* device,
                                                         WGPUErrorType type, WGPUStringView message,
                                                         void* userdata1, void* userdata2) {
        std::cerr << "WebGPU Uncaptured Error (Type " << type << "): ";
        if (message.data && message.length > 0) {
            std::cerr.write(message.data, message.length);
        } else {
            std::cerr << "Unknown error";
        }
        std::cerr << std::endl;
    };
    deviceDesc.uncapturedErrorCallbackInfo.userdata1 = nullptr;

    // Setup device lost callback
    deviceDesc.deviceLostCallbackInfo.callback = [](WGPUDevice const* device,
                                                     WGPUDeviceLostReason reason, WGPUStringView message,
                                                     void* userdata1, void* userdata2) {
        std::cerr << "WebGPU Device Lost (Reason " << reason << "): ";
        if (message.data && message.length > 0) {
            std::cerr.write(message.data, message.length);
        } else {
            std::cerr << "Unknown reason";
        }
        std::cerr << std::endl;
    };
    deviceDesc.deviceLostCallbackInfo.userdata1 = nullptr;

    struct DeviceRequestResult {
        WGPUDevice device = nullptr;
        bool completed = false;
    } deviceResult;

    WGPURequestDeviceCallbackInfo deviceCallbackInfo = {};
    deviceCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    deviceCallbackInfo.callback = [](WGPURequestDeviceStatus status, WGPUDevice device,
                                     WGPUStringView message, void* userdata1, void* userdata2) {
        auto* result = static_cast<DeviceRequestResult*>(userdata1);
        if (status == WGPURequestDeviceStatus_Success) {
            result->device = device;
        } else {
            std::cerr << "Request device failed: ";
            if (message.data && message.length > 0) {
                std::cerr.write(message.data, message.length);
            } else {
                std::cerr << "Unknown error";
            }
            std::cerr << std::endl;
        }
        result->completed = true;
    };
    deviceCallbackInfo.userdata1 = &deviceResult;

    WGPUFuture deviceFuture = wgpuAdapterRequestDevice(mAdapter, &deviceDesc, deviceCallbackInfo);

    WGPUFutureWaitInfo deviceWaitInfo = {};
    deviceWaitInfo.future = deviceFuture;
    deviceWaitInfo.completed = false;

    while (!deviceResult.completed) {
        wgpuInstanceWaitAny(mInstance, 1, &deviceWaitInfo, UINT64_MAX);
    }

    mDevice = deviceResult.device;
    if (!mDevice) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    mQueue = wgpuDeviceGetQueue(mDevice);

    // Initial Swapchain creation
    return CreateSwapChain(mWidth, mHeight);
}

bool NativeContext::CreateSwapChain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return false;
    }

    mWidth = width;
    mHeight = height;

    WGPUSurfaceConfiguration config = {};
    config.device = mDevice;
    config.format = mSwapChainFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = mWidth;
    config.height = mHeight;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(mSurface, &config);
    return true;
}

WGPUTextureView NativeContext::GetCurrentSwapChainView() {
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(mSurface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << surfaceTexture.status
                  << std::endl;
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = {"SwapChain View", WGPU_STRLEN};
    viewDesc.format = mSwapChainFormat;
    viewDesc.dimension = WGPUTextureViewDimension_2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipLevelCount = 1;
    viewDesc.baseArrayLayer = 0;
    viewDesc.arrayLayerCount = 1;
    viewDesc.aspect = WGPUTextureAspect_All;

    WGPUTextureView view = wgpuTextureCreateView(surfaceTexture.texture, &viewDesc);

    // Release the texture reference returned by wgpuSurfaceGetCurrentTexture
    wgpuTextureRelease(surfaceTexture.texture);

    return view;
}

void NativeContext::Present() {
    wgpuSurfacePresent(mSurface);
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
        wgpuInstanceProcessEvents(mInstance);
    }
}
