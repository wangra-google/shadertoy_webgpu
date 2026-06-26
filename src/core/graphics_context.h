#ifndef GRAPHICS_CONTEXT_H
#define GRAPHICS_CONTEXT_H

#include <stdint.h>
#include <webgpu/webgpu.h>

class GraphicsContext {
  public:
    virtual ~GraphicsContext() = default;

    // Lifecycle Initializers
    virtual bool Init() = 0;
    virtual bool CreateSwapChain(uint32_t width, uint32_t height) = 0;

    // Context Getters
    virtual WGPUDevice GetDevice() const = 0;
    virtual WGPUQueue GetQueue() const = 0;
    virtual WGPUSurface GetSurface() const = 0;
    virtual WGPUTextureFormat GetSwapChainFormat() const = 0;

    // Frame Execution Hooks
    virtual WGPUTextureView GetCurrentSwapChainView() = 0;
    virtual void Present() = 0;

    // Screen Properties
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
};

#endif  // GRAPHICS_CONTEXT_H
