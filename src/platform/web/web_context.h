#ifndef PLATFORM_WEB_WEB_CONTEXT_H_
#define PLATFORM_WEB_WEB_CONTEXT_H_

#include <cstdint>
#include <webgpu/webgpu.h>

#include <string>

#include "core/config.h"
#include "core/graphics_context.h"

class WebContext : public GraphicsContext {
  public:
    WebContext(const std::string& canvasSelector, uint32_t width, uint32_t height);
    ~WebContext() override;

    bool Init() override;
    bool CreateSwapChain(uint32_t width, uint32_t height) override;

    WGPUDevice GetDevice() const override { return mDevice; }
    WGPUQueue GetQueue() const override { return mQueue; }
    WGPUSurface GetSurface() const override { return mSurface; }
    WGPUTextureFormat GetSwapChainFormat() const override { return mSwapChainFormat; }

    WGPUTextureView GetCurrentSwapChainView() override;
    void Present() override;

    uint32_t GetWidth() const override { return mWidth; }
    uint32_t GetHeight() const override { return mHeight; }

  private:
    std::string mCanvasSelector;
    uint32_t mWidth = config::kWebDefaultWidth;
    uint32_t mHeight = config::kWebDefaultHeight;

    WGPUInstance mInstance = nullptr;
    WGPUSurface mSurface = nullptr;
    WGPUAdapter mAdapter = nullptr;
    WGPUDevice mDevice = nullptr;
    WGPUQueue mQueue = nullptr;
    WGPUTextureFormat mSwapChainFormat = WGPUTextureFormat_RGBA8Unorm;
};

#endif  // PLATFORM_WEB_WEB_CONTEXT_H_
