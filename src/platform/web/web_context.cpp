#include "platform/web/web_context.h"

#include <emscripten/emscripten.h>

#include <iostream>

WebContext::WebContext(const std::string& canvasSelector, uint32_t width, uint32_t height)
    : mCanvasSelector(canvasSelector), mWidth(width), mHeight(height) {}

WebContext::~WebContext() {
    if (mSurface) {
        mSurface.Unconfigure();
    }
}

bool WebContext::Init() {
    // Create Instance
    wgpu::InstanceDescriptor instanceDesc;
    mInstance = wgpu::CreateInstance(&instanceDesc);
    if (!mInstance) {
        std::cerr << "Failed to create WGPU Instance!" << std::endl;
        return false;
    }

    // Create Surface from Canvas HTML Selector using Emscripten specific extension struct
    wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc;
    canvasDesc.selector = mCanvasSelector.c_str();

    wgpu::SurfaceDescriptor surfaceDesc;
    surfaceDesc.nextInChain = &canvasDesc;
    mSurface = mInstance.CreateSurface(&surfaceDesc);
    if (!mSurface) {
        std::cerr << "Failed to create WGPU Surface from canvas selector!" << std::endl;
        return false;
    }

    // Request Adapter
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = mSurface;
    options.powerPreference = wgpu::PowerPreference::HighPerformance;

    wgpu::Adapter adapter = nullptr;
    bool adapterCompleted = false;

    wgpu::Future adapterFuture = mInstance.RequestAdapter(
        &options,
        wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapterIn, const char* message) {
            if (status == wgpu::RequestAdapterStatus::Success) {
                adapter = adapterIn;
            } else {
                std::cerr << "Request adapter failed: " << (message ? message : "Unknown error") << std::endl;
            }
            adapterCompleted = true;
        }
    );

    wgpu::FutureWaitInfo adapterWaitInfo = {};
    adapterWaitInfo.future = adapterFuture;
    adapterWaitInfo.completed = false;

    while (!adapterCompleted) {
        mInstance.WaitAny(1, &adapterWaitInfo, 0);
        emscripten_sleep(1);
    }

    mAdapter = adapter;
    if (!mAdapter) {
        std::cerr << "No compatible WGPU Adapter found!" << std::endl;
        return false;
    }

    // Request Logical Device
    wgpu::DeviceDescriptor deviceDesc;
    deviceDesc.label = "Web Logical Device";

    deviceDesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView message) {
            std::cerr << "WebGPU Uncaptured Error (Type " << static_cast<int>(type) << "): "
                      << (message.data ? std::string_view(message.data, message.length) : "Unknown error")
                      << std::endl;
        });

    wgpu::Device device = nullptr;
    bool deviceCompleted = false;

    wgpu::Future deviceFuture = mAdapter.RequestDevice(
        &deviceDesc,
        wgpu::CallbackMode::AllowSpontaneous,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device deviceIn, const char* message) {
            if (status == wgpu::RequestDeviceStatus::Success) {
                device = deviceIn;
            } else {
                std::cerr << "Request device failed: " << (message ? message : "Unknown error") << std::endl;
            }
            deviceCompleted = true;
        }
    );

    wgpu::FutureWaitInfo deviceWaitInfo = {};
    deviceWaitInfo.future = deviceFuture;
    deviceWaitInfo.completed = false;

    while (!deviceCompleted) {
        mInstance.WaitAny(1, &deviceWaitInfo, 0);
        emscripten_sleep(1);
    }

    mDevice = device;
    if (!mDevice) {
        std::cerr << "Failed to create WGPU Device!" << std::endl;
        return false;
    }

    mQueue = mDevice.GetQueue();

    return CreateSwapChain(mWidth, mHeight);
}

bool WebContext::CreateSwapChain(uint32_t width, uint32_t height) {
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

wgpu::TextureView WebContext::GetCurrentSwapChainView() {
    wgpu::SurfaceTexture surfaceTexture = {};
    mSurface.GetCurrentTexture(&surfaceTexture);

    if (surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surfaceTexture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
        std::cerr << "Failed to get current surface texture! Status: " << static_cast<int>(surfaceTexture.status)
                  << std::endl;
        return nullptr;
    }

    wgpu::TextureViewDescriptor viewDesc;
    viewDesc.label = "Web SwapChain View";
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

void WebContext::Present() {
    // No-op on the web. The browser presents the canvas automatically at the end of the frame.
}
