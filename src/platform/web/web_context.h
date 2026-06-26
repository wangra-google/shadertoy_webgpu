#ifndef WEB_CONTEXT_H
#define WEB_CONTEXT_H

#include <stdint.h>
#include <webgpu/webgpu.h>

#include <string>

#include "core/config.h"
#include "core/graphics_context.h"

class WebContext : public GraphicsContext {
  public:
    WebContext(const std::string& canvasSelector);
    ~WebContext() override;

    bool Init() override;
    bool CreateSwapChain(uint32_t width, uint32_t height) override;

    WGPUDevice GetDevice() const override { return m_device; }
    WGPUQueue GetQueue() const override { return m_queue; }
    WGPUSurface GetSurface() const override { return m_surface; }
    WGPUTextureFormat GetSwapChainFormat() const override { return m_swapChainFormat; }

    WGPUTextureView GetCurrentSwapChainView() override;
    void Present() override;

    uint32_t GetWidth() const override { return m_width; }
    uint32_t GetHeight() const override { return m_height; }

  private:
    std::string m_canvasSelector;
    uint32_t m_width = config::kWebDefaultWidth;
    uint32_t m_height = config::kWebDefaultHeight;

    WGPUInstance m_instance = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_RGBA8Unorm;
};

#endif  // WEB_CONTEXT_H
