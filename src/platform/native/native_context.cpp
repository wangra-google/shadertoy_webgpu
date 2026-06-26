#include "platform/native/native_context.h"

#include <iostream>
#include <vector>

#include "core/config.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <dawn/dawn_proc.h>
#include <dawn/native/DawnNative.h>
#include <windows.h>

NativeContext::NativeContext(uint32_t width, uint32_t height) : m_width(width), m_height(height) {}

NativeContext::~NativeContext() {
    if (m_surface) {
        wgpuSurfaceUnconfigure(m_surface);
        wgpuSurfaceRelease(m_surface);
    }
    if (m_queue) {
        wgpuQueueRelease(m_queue);
    }
    if (m_device) {
        wgpuDeviceRelease(m_device);
    }
    if (m_adapter) {
        wgpuAdapterRelease(m_adapter);
    }
    if (m_instance) {
        wgpuInstanceRelease(m_instance);
    }

    if (m_window) {
        glfwDestroyWindow(m_window);
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

    m_window = glfwCreateWindow(m_width, m_height, config::kNativeWindowTitle, nullptr, nullptr);
    if (!m_window) {
        std::cerr << "Failed to create GLFW window!" << std::endl;
        glfwTerminate();
        return false;
    }

    // Set user pointer to context for callbacks
    glfwSetWindowUserPointer(m_window, this);

    // Create Instance
    WGPUInstanceDescriptor instanceDesc = {};
    m_instance = wgpuCreateInstance(&instanceDesc);
    if (!m_instance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface from HWND
    HWND hwnd = glfwGetWin32Window(m_window);
    HINSTANCE hinstance = GetModuleHandle(nullptr);

    WGPUSurfaceSourceWindowsHWND winDesc = {};
    winDesc.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    winDesc.chain.next = nullptr;
    winDesc.hwnd = hwnd;
    winDesc.hinstance = hinstance;

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &winDesc.chain;
    m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
    if (!m_surface) {
        std::cerr << "Failed to create WGPU Surface!" << std::endl;
        return false;
    }

    // Request Adapter
    WGPURequestAdapterOptions options = {};
    options.compatibleSurface = m_surface;
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
        wgpuInstanceRequestAdapter(m_instance, &options, adapterCallbackInfo);

    WGPUFutureWaitInfo adapterWaitInfo = {};
    adapterWaitInfo.future = adapterFuture;
    adapterWaitInfo.completed = false;

    while (!adapterResult.completed) {
        wgpuInstanceWaitAny(m_instance, 1, &adapterWaitInfo, UINT64_MAX);
    }

    m_adapter = adapterResult.adapter;
    if (!m_adapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Enumerate features to check for optional TimestampQuery
    std::vector<WGPUFeatureName> requiredFeatures;
    WGPUSupportedFeatures supportedFeatures = {};
    wgpuAdapterGetFeatures(m_adapter, &supportedFeatures);
    for (size_t i = 0; i < supportedFeatures.featureCount; ++i) {
        if (supportedFeatures.features[i] == WGPUFeatureName_TimestampQuery) {
            requiredFeatures.push_back(WGPUFeatureName_TimestampQuery);
            m_timestampsSupported = true;
            break;
        }
    }

    // Request Logical Device
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = {"Renderer Logical Device", WGPU_STRLEN};
    deviceDesc.requiredFeatureCount = requiredFeatures.size();
    deviceDesc.requiredFeatures = requiredFeatures.data();

    // Setup uncaptured error callback for validation feedback
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

    WGPUFuture deviceFuture = wgpuAdapterRequestDevice(m_adapter, &deviceDesc, deviceCallbackInfo);

    WGPUFutureWaitInfo deviceWaitInfo = {};
    deviceWaitInfo.future = deviceFuture;
    deviceWaitInfo.completed = false;

    while (!deviceResult.completed) {
        wgpuInstanceWaitAny(m_instance, 1, &deviceWaitInfo, UINT64_MAX);
    }

    m_device = deviceResult.device;
    if (!m_device) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    m_queue = wgpuDeviceGetQueue(m_device);

    // Initial Swapchain creation
    return CreateSwapChain(m_width, m_height);
}

bool NativeContext::CreateSwapChain(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        return false;
    }

    m_width = width;
    m_height = height;

    WGPUSurfaceConfiguration config = {};
    config.device = m_device;
    config.format = m_swapChainFormat;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = m_width;
    config.height = m_height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Auto;

    wgpuSurfaceConfigure(m_surface, &config);
    return true;
}

WGPUTextureView NativeContext::GetCurrentSwapChainView() {
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << surfaceTexture.status
                  << std::endl;
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = {"SwapChain View", WGPU_STRLEN};
    viewDesc.format = m_swapChainFormat;
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
    wgpuSurfacePresent(m_surface);
}

bool NativeContext::ShouldClose() const {
    if (!m_window) {
        return true;
    }
    return glfwWindowShouldClose(m_window);
}

void NativeContext::PollEvents() {
    glfwPollEvents();
}
