#include "core/texture_manager.h"
#include <iostream>
#include <cstring>

TextureManager::TextureManager(wgpu::Device device) : mDevice(device) {
    CreateDefaultResources();
}

TextureManager::~TextureManager() {
    // Release static textures
    for (auto& pair : mStaticTextures) {
        ReleaseTextureResource(pair.second);
    }
    mStaticTextures.clear();

    // Release render targets
    for (auto& pair : mRenderTargets) {
        ReleaseTextureResource(pair.second.resources[0]);
        ReleaseTextureResource(pair.second.resources[1]);
    }
    mRenderTargets.clear();

    // Release default resources
    if (mDefaultTexture) {
        mDefaultTexture.Destroy();
    }
}

void TextureManager::ReleaseTextureResource(TextureResource& res) {
    // Automatic reference count release on assignment of nullptr
    res.view = nullptr;
    res.texture = nullptr;
}

bool TextureManager::AllocateTextureResource(TextureResource& res, uint32_t width, uint32_t height, wgpu::TextureFormat format, wgpu::TextureUsage usage) {
    ReleaseTextureResource(res);

    wgpu::TextureDescriptor desc;
    desc.dimension = wgpu::TextureDimension::e2D;
    desc.size = {width, height, 1};
    desc.format = format;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.usage = usage;

    res.texture = mDevice.CreateTexture(&desc);
    if (!res.texture) {
        std::cerr << "TextureManager: Failed to create texture of size " << width << "x" << height << std::endl;
        return false;
    }

    res.view = res.texture.CreateView();
    if (!res.view) {
        std::cerr << "TextureManager: Failed to create texture view." << std::endl;
        res.texture = nullptr;
        return false;
    }

    res.width = width;
    res.height = height;
    res.format = format;
    return true;
}

void TextureManager::CreateDefaultResources() {
    // 1x1 Black Texture
    uint32_t blackPixel = 0xFF000000; // RGBA8 black
    wgpu::TextureUsage usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    
    TextureResource defaultRes;
    if (AllocateTextureResource(defaultRes, 1, 1, wgpu::TextureFormat::RGBA8Unorm, usage)) {
        mDefaultTexture = defaultRes.texture;
        mDefaultTextureView = defaultRes.view;
        
        wgpu::TexelCopyTextureInfo destination = {};
        destination.texture = mDefaultTexture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = wgpu::TextureAspect::All;

        wgpu::TexelCopyBufferLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = 4;
        dataLayout.rowsPerImage = 1;

        wgpu::Extent3D writeSize = {1, 1, 1};

        wgpu::Queue queue = mDevice.GetQueue();
        queue.WriteTexture(&destination, &blackPixel, 4, &dataLayout, &writeSize);
    }

    // Default Sampler (Bilinear, Repeat)
    wgpu::SamplerDescriptor samplerDesc;
    samplerDesc.addressModeU = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeV = wgpu::AddressMode::Repeat;
    samplerDesc.addressModeW = wgpu::AddressMode::Repeat;
    samplerDesc.magFilter = wgpu::FilterMode::Linear;
    samplerDesc.minFilter = wgpu::FilterMode::Linear;
    samplerDesc.mipmapFilter = wgpu::MipmapFilterMode::Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.maxAnisotropy = 1;
    samplerDesc.compare = wgpu::CompareFunction::Undefined;

    mDefaultSampler = mDevice.CreateSampler(&samplerDesc);
}

bool TextureManager::CreateStaticTexture(const std::string& name, const void* pixels, uint32_t width, uint32_t height, wgpu::TextureFormat format) {
    wgpu::TextureUsage usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    
    TextureResource res;
    if (!AllocateTextureResource(res, width, height, format, usage)) {
        return false;
    }

    wgpu::TexelCopyTextureInfo destination = {};
    destination.texture = res.texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = wgpu::TextureAspect::All;

    // Calculate bytes per row based on format
    uint32_t bytesPerPixel = 4; // Default RGBA8
    if (format == wgpu::TextureFormat::RGBA16Float) {
        bytesPerPixel = 8;
    }

    wgpu::TexelCopyBufferLayout dataLayout = {};
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = width * bytesPerPixel;
    dataLayout.rowsPerImage = height;

    wgpu::Extent3D writeSize = {width, height, 1};

    wgpu::Queue queue = mDevice.GetQueue();
    queue.WriteTexture(&destination, pixels, width * height * bytesPerPixel, &dataLayout, &writeSize);

    // If it already exists, release the old one first
    auto it = mStaticTextures.find(name);
    if (it != mStaticTextures.end()) {
        ReleaseTextureResource(it->second);
    }
    mStaticTextures[name] = res;
    return true;
}

bool TextureManager::CreateRenderTarget(const std::string& name, uint32_t width, uint32_t height, wgpu::TextureFormat format, bool pingPong, bool isDynamic) {
    wgpu::TextureUsage usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;

    PingPongTexture ppTex;
    ppTex.isPingPong = pingPong;
    ppTex.writeIndex = 0;

    if (isDynamic) {
        // Find if it's already in the dynamic list
        bool found = false;
        for (const auto& info : mDynamicRenderTargets) {
            if (info.name == name) {
                found = true;
                break;
            }
        }
        if (!found) {
            mDynamicRenderTargets.push_back({name, format, pingPong});
        }
        // Set temporary size if not specified (will be resized immediately by ResizeRenderTargets)
        if (width == 0) width = 1;
        if (height == 0) height = 1;
    }

    if (!AllocateTextureResource(ppTex.resources[0], width, height, format, usage)) {
        return false;
    }

    if (pingPong) {
        if (!AllocateTextureResource(ppTex.resources[1], width, height, format, usage)) {
            ReleaseTextureResource(ppTex.resources[0]);
            return false;
        }
    }

    auto it = mRenderTargets.find(name);
    if (it != mRenderTargets.end()) {
        ReleaseTextureResource(it->second.resources[0]);
        ReleaseTextureResource(it->second.resources[1]);
    }
    mRenderTargets[name] = ppTex;
    return true;
}

void TextureManager::ResizeRenderTargets(uint32_t width, uint32_t height) {
    if (width == 0) width = 1;
    if (height == 0) height = 1;

    wgpu::TextureUsage usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::CopySrc;

    for (const auto& info : mDynamicRenderTargets) {
        auto it = mRenderTargets.find(info.name);
        if (it != mRenderTargets.end()) {
            PingPongTexture& ppTex = it->second;
            
            // Reallocate resource 0
            AllocateTextureResource(ppTex.resources[0], width, height, info.format, usage);
            
            // Reallocate resource 1 if ping-pong
            if (info.pingPong) {
                AllocateTextureResource(ppTex.resources[1], width, height, info.format, usage);
            }
            
            ppTex.writeIndex = 0;
        }
    }
}

wgpu::TextureView TextureManager::GetReadView(const std::string& name) const {
    // Check static textures first
    auto itStatic = mStaticTextures.find(name);
    if (itStatic != mStaticTextures.end()) {
        return itStatic->second.view;
    }

    // Check render targets
    auto itRT = mRenderTargets.find(name);
    if (itRT != mRenderTargets.end()) {
        return itRT->second.GetReadResource().view;
    }

    // Fallback to default black texture
    return mDefaultTextureView;
}

wgpu::TextureView TextureManager::GetWriteView(const std::string& name) const {
    auto itRT = mRenderTargets.find(name);
    if (itRT != mRenderTargets.end()) {
        return itRT->second.GetWriteResource().view;
    }
    return nullptr;
}

void TextureManager::SwapPingPong(const std::string& name) {
    auto itRT = mRenderTargets.find(name);
    if (itRT != mRenderTargets.end()) {
        itRT->second.Swap();
    }
}

wgpu::TextureFormat TextureManager::GetFormat(const std::string& name) const {
    auto itRT = mRenderTargets.find(name);
    if (itRT != mRenderTargets.end()) {
        return itRT->second.resources[0].format;
    }
    auto itStatic = mStaticTextures.find(name);
    if (itStatic != mStaticTextures.end()) {
        return itStatic->second.format;
    }
    return wgpu::TextureFormat::Undefined;
}
