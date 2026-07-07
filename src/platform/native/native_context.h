#ifndef PLATFORM_NATIVE_NATIVE_CONTEXT_H_
#define PLATFORM_NATIVE_NATIVE_CONTEXT_H_

#include <cstdint>
#include <webgpu/webgpu_cpp.h>

#include "core/graphics_context.h"

struct GLFWwindow;

class NativeContext : public GraphicsContext {
  public:
    NativeContext(uint32_t width, uint32_t height);
    ~NativeContext() override;

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

    GLFWwindow* GetWindow() const { return mWindow; }
    bool ShouldClose() const;
    void PollEvents();

    bool IsTimestampsSupported() const { return mTimestampsSupported; }

  private:
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    GLFWwindow* mWindow = nullptr;

    wgpu::Instance mInstance = nullptr;
    wgpu::Surface mSurface = nullptr;
    wgpu::Adapter mAdapter = nullptr;
    wgpu::Device mDevice = nullptr;
    wgpu::Queue mQueue = nullptr;
    wgpu::TextureFormat mSwapChainFormat = wgpu::TextureFormat::BGRA8Unorm;

    bool mTimestampsSupported = false;
};

#endif  // PLATFORM_NATIVE_NATIVE_CONTEXT_H_
