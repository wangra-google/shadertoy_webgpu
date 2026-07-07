#ifndef PLATFORM_WEB_WEB_CONTEXT_H_
#define PLATFORM_WEB_WEB_CONTEXT_H_

#include <cstdint>
#include <webgpu/webgpu_cpp.h>

#include <string>

#include "core/config.h"
#include "core/graphics_context.h"

class WebContext : public GraphicsContext {
  public:
    WebContext(const std::string& canvasSelector, uint32_t width, uint32_t height);
    ~WebContext() override;

    bool Init() override;
    bool CreateSwapChain(uint32_t width, uint32_t height) override;

    wgpu::Device GetDevice() const override { return mDevice; }
    wgpu::Queue GetQueue() const override { return mQueue; }
    wgpu::Surface GetSurface() const override { return mSurface; }
    wgpu::TextureFormat GetSwapChainFormat() const override { return mSwapChainFormat; }

    wgpu::TextureView GetCurrentSwapChainView() override;
    void Present() override;

    uint32_t GetWidth() const override { return mWidth; }
    uint32_t GetHeight() const override { return mHeight; }

  private:
    std::string mCanvasSelector;
    uint32_t mWidth = config::kWebDefaultWidth;
    uint32_t mHeight = config::kWebDefaultHeight;

    wgpu::Instance mInstance = nullptr;
    wgpu::Surface mSurface = nullptr;
    wgpu::Adapter mAdapter = nullptr;
    wgpu::Device mDevice = nullptr;
    wgpu::Queue mQueue = nullptr;
    wgpu::TextureFormat mSwapChainFormat = wgpu::TextureFormat::RGBA8Unorm;
};

#endif  // PLATFORM_WEB_WEB_CONTEXT_H_
