#ifndef PLATFORM_NATIVE_NATIVE_CONTEXT_H_
#define PLATFORM_NATIVE_NATIVE_CONTEXT_H_

#include <cstdint>
#include <webgpu/webgpu.h>

#include "core/graphics_context.h"

struct GLFWwindow;

class NativeContext : public GraphicsContext {
  public:
    NativeContext(uint32_t width, uint32_t height);
    ~NativeContext() override;

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

    GLFWwindow* GetWindow() const { return mWindow; }
    bool ShouldClose() const;
    void PollEvents();

    bool IsTimestampsSupported() const { return mTimestampsSupported; }

  private:
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    GLFWwindow* mWindow = nullptr;

    WGPUInstance mInstance = nullptr;
    WGPUSurface mSurface = nullptr;
    WGPUAdapter mAdapter = nullptr;
    WGPUDevice mDevice = nullptr;
    WGPUQueue mQueue = nullptr;
    WGPUTextureFormat mSwapChainFormat = WGPUTextureFormat_BGRA8Unorm;

    bool mTimestampsSupported = false;
};

#endif  // PLATFORM_NATIVE_NATIVE_CONTEXT_H_
