#ifndef CORE_TEXTURE_MANAGER_H_
#define CORE_TEXTURE_MANAGER_H_

#include <string>
#include <unordered_map>
#include <vector>
#include <webgpu/webgpu.h>

struct TextureResource {
    WGPUTexture texture = nullptr;
    WGPUTextureView view = nullptr;
    uint32_t width = 0;
    uint32_t height = 0;
    WGPUTextureFormat format = WGPUTextureFormat_RGBA8Unorm;
};

struct PingPongTexture {
    TextureResource resources[2];
    uint32_t writeIndex = 0; // 0 or 1. The other is read.
    bool isPingPong = false;
    
    void Swap() {
        if (isPingPong) {
            writeIndex = 1 - writeIndex;
        }
    }
    
    const TextureResource& GetReadResource() const {
        return resources[1 - writeIndex];
    }
    
    const TextureResource& GetWriteResource() const {
        return resources[writeIndex];
    }
};

class TextureManager {
  public:
    explicit TextureManager(WGPUDevice device);
    ~TextureManager();

    // Create or update a static texture with CPU pixel data
    bool CreateStaticTexture(const std::string& name, const void* pixels, uint32_t width, uint32_t height, WGPUTextureFormat format);

    // Create or update a render target texture (optionally ping-pong)
    bool CreateRenderTarget(const std::string& name, uint32_t width, uint32_t height, WGPUTextureFormat format, bool pingPong);

    // Resize all render targets that match the viewport size (width=0, height=0 means dynamic)
    void ResizeRenderTargets(uint32_t width, uint32_t height);

    // Get the texture view for reading (in a bind group)
    WGPUTextureView GetReadView(const std::string& name) const;

    // Get the texture view for writing (as a render target attachment)
    WGPUTextureView GetWriteView(const std::string& name) const;

    // Get the format of a texture (static or render target)
    WGPUTextureFormat GetFormat(const std::string& name) const;

    // Swap ping-pong buffers at the end of a frame or pass
    void SwapPingPong(const std::string& name);
    
    // Get a default 1x1 black texture view (to bind to unused channels)
    WGPUTextureView GetDefaultView() const { return mDefaultTextureView; }
    
    // Get default sampler
    WGPUSampler GetDefaultSampler() const { return mDefaultSampler; }

  private:
    void ReleaseTextureResource(TextureResource& res);
    bool AllocateTextureResource(TextureResource& res, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage);
    void CreateDefaultResources();

    WGPUDevice mDevice;
    
    std::unordered_map<std::string, TextureResource> mStaticTextures;
    std::unordered_map<std::string, PingPongTexture> mRenderTargets;
    
    // Track which render targets are dynamic (resize with viewport)
    struct DynamicRenderTargetInfo {
        std::string name;
        WGPUTextureFormat format;
        bool pingPong;
    };
    std::vector<DynamicRenderTargetInfo> mDynamicRenderTargets;

    // Default 1x1 black texture for unbound slots
    WGPUTexture mDefaultTexture = nullptr;
    WGPUTextureView mDefaultTextureView = nullptr;
    
    // Default bilinear sampler
    WGPUSampler mDefaultSampler = nullptr;
};

#endif // CORE_TEXTURE_MANAGER_H_
