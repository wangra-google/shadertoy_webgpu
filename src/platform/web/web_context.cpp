#include "platform/web/web_context.h"

#include <emscripten/emscripten.h>

#include <iostream>

WebContext::WebContext(const std::string& canvasSelector) : m_canvasSelector(canvasSelector) {}

WebContext::~WebContext() {
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
}

bool WebContext::Init() {
    // Create Instance
    WGPUInstanceDescriptor instanceDesc = {};
    m_instance = wgpuCreateInstance(&instanceDesc);
    if (!m_instance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface from Canvas HTML Selector
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
    canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasDesc.chain.next = nullptr;
    canvasDesc.selector = {m_canvasSelector.c_str(), WGPU_STRLEN};

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &canvasDesc.chain;
    m_surface = wgpuInstanceCreateSurface(m_instance, &surfaceDesc);
    if (!m_surface) {
        std::cerr << "Failed to create WGPU Surface from canvas selector!" << std::endl;
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
        wgpuInstanceWaitAny(m_instance, 1, &adapterWaitInfo, 0);
        emscripten_sleep(1);
    }

    m_adapter = adapterResult.adapter;
    if (!m_adapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Request Logical Device
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = {"Web Logical Device", WGPU_STRLEN};

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
        wgpuInstanceWaitAny(m_instance, 1, &deviceWaitInfo, 0);
        emscripten_sleep(1);
    }

    m_device = deviceResult.device;
    if (!m_device) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    m_queue = wgpuDeviceGetQueue(m_device);

    return CreateSwapChain(m_width, m_height);
}

bool WebContext::CreateSwapChain(uint32_t width, uint32_t height) {
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

WGPUTextureView WebContext::GetCurrentSwapChainView() {
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(m_surface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << surfaceTexture.status
                  << std::endl;
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = {"Web SwapChain View", WGPU_STRLEN};
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

void WebContext::Present() {
    // No-op on the web. The browser presents the canvas automatically at the end of the frame.
}
