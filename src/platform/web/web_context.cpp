#include "platform/web/web_context.h"

#include <emscripten/emscripten.h>

#include <iostream>

WebContext::WebContext(const std::string& canvasSelector, uint32_t width, uint32_t height)
    : mCanvasSelector(canvasSelector), mWidth(width), mHeight(height) {}

WebContext::~WebContext() {
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
}

bool WebContext::Init() {
    // Create Instance
    WGPUInstanceDescriptor instanceDesc = {};
    mInstance = wgpuCreateInstance(&instanceDesc);
    if (!mInstance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface from Canvas HTML Selector
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
    canvasDesc.chain.sType = WGPUSType_EmscriptenSurfaceSourceCanvasHTMLSelector;
    canvasDesc.chain.next = nullptr;
    canvasDesc.selector = {mCanvasSelector.c_str(), WGPU_STRLEN};

    WGPUSurfaceDescriptor surfaceDesc = {};
    surfaceDesc.nextInChain = &canvasDesc.chain;
    mSurface = wgpuInstanceCreateSurface(mInstance, &surfaceDesc);
    if (!mSurface) {
        std::cerr << "Failed to create WGPU Surface from canvas selector!" << std::endl;
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
        wgpuInstanceWaitAny(mInstance, 1, &adapterWaitInfo, 0);
        emscripten_sleep(1);
    }

    mAdapter = adapterResult.adapter;
    if (!mAdapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Request Logical Device
    WGPUDeviceDescriptor deviceDesc = {};
    deviceDesc.label = {"Web Logical Device", WGPU_STRLEN};

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
        wgpuInstanceWaitAny(mInstance, 1, &deviceWaitInfo, 0);
        emscripten_sleep(1);
    }

    mDevice = deviceResult.device;
    if (!mDevice) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    mQueue = wgpuDeviceGetQueue(mDevice);

    return CreateSwapChain(mWidth, mHeight);
}

bool WebContext::CreateSwapChain(uint32_t width, uint32_t height) {
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

WGPUTextureView WebContext::GetCurrentSwapChainView() {
    WGPUSurfaceTexture surfaceTexture = {};
    wgpuSurfaceGetCurrentTexture(mSurface, &surfaceTexture);

    if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
        surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << surfaceTexture.status
                  << std::endl;
        return nullptr;
    }

    WGPUTextureViewDescriptor viewDesc = {};
    viewDesc.label = {"Web SwapChain View", WGPU_STRLEN};
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

void WebContext::Present() {
    // No-op on the web. The browser presents the canvas automatically at the end of the frame.
}
