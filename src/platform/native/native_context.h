#ifndef NATIVE_CONTEXT_H
#define NATIVE_CONTEXT_H

#include <stdint.h>
#include <webgpu/webgpu.h>

#include "core/graphics_context.h"

struct GLFWwindow;

class NativeContext : public GraphicsContext {
  public:
    NativeContext(uint32_t width, uint32_t height);
    ~NativeContext() override;

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

    GLFWwindow* GetWindow() const { return m_window; }
    bool ShouldClose() const;
    void PollEvents();

    bool IsTimestampsSupported() const { return m_timestampsSupported; }

  private:
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    GLFWwindow* m_window = nullptr;

    WGPUInstance m_instance = nullptr;
    WGPUSurface m_surface = nullptr;
    WGPUAdapter m_adapter = nullptr;
    WGPUDevice m_device = nullptr;
    WGPUQueue m_queue = nullptr;
    WGPUTextureFormat m_swapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    bool m_timestampsSupported = false;
};

#endif  // NATIVE_CONTEXT_H
