#ifndef CORE_GRAPHICS_CONTEXT_H_
#define CORE_GRAPHICS_CONTEXT_H_

#include <cstdint>
#include <webgpu/webgpu_cpp.h>

class GraphicsContext {
  public:
    virtual ~GraphicsContext() = default;

    // Lifecycle Initializers
    virtual bool Init() = 0;
    virtual bool CreateSwapChain(uint32_t width, uint32_t height) = 0;

    // Context Getters
    virtual wgpu::Device GetDevice() const = 0;
    virtual wgpu::Queue GetQueue() const = 0;
    virtual wgpu::Surface GetSurface() const = 0;
    virtual wgpu::TextureFormat GetSwapChainFormat() const = 0;

    // Frame Execution Hooks
    virtual wgpu::TextureView GetCurrentSwapChainView() = 0;
    virtual void Present() = 0;

    // Screen Properties
    virtual uint32_t GetWidth() const = 0;
    virtual uint32_t GetHeight() const = 0;
};

#endif  // CORE_GRAPHICS_CONTEXT_H_

