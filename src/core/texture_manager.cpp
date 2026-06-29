#include "core/texture_manager.h"
#include <iostream>
#include <cstring>

TextureManager::TextureManager(WGPUDevice device) : mDevice(device) {
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
    if (mDefaultTextureView) {
        wgpuTextureViewRelease(mDefaultTextureView);
    }
    if (mDefaultTexture) {
        wgpuTextureDestroy(mDefaultTexture);
        wgpuTextureRelease(mDefaultTexture);
    }
    if (mDefaultSampler) {
        wgpuSamplerRelease(mDefaultSampler);
    }
}

void TextureManager::ReleaseTextureResource(TextureResource& res) {
    if (res.view) {
        wgpuTextureViewRelease(res.view);
        res.view = nullptr;
    }
    if (res.texture) {
        wgpuTextureDestroy(res.texture);
        wgpuTextureRelease(res.texture);
        res.texture = nullptr;
    }
}

bool TextureManager::AllocateTextureResource(TextureResource& res, uint32_t width, uint32_t height, WGPUTextureFormat format, WGPUTextureUsage usage) {
    ReleaseTextureResource(res);

    WGPUTextureDescriptor desc = {};
    desc.dimension = WGPUTextureDimension_2D;
    desc.size = {width, height, 1};
    desc.format = format;
    desc.mipLevelCount = 1;
    desc.sampleCount = 1;
    desc.usage = usage;

    res.texture = wgpuDeviceCreateTexture(mDevice, &desc);
    if (!res.texture) {
        std::cerr << "TextureManager: Failed to create texture of size " << width << "x" << height << std::endl;
        return false;
    }

    res.view = wgpuTextureCreateView(res.texture, nullptr);
    if (!res.view) {
        std::cerr << "TextureManager: Failed to create texture view." << std::endl;
        wgpuTextureDestroy(res.texture);
        wgpuTextureRelease(res.texture);
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
    WGPUTextureUsage usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    
    TextureResource defaultRes;
    if (AllocateTextureResource(defaultRes, 1, 1, WGPUTextureFormat_RGBA8Unorm, usage)) {
        mDefaultTexture = defaultRes.texture;
        mDefaultTextureView = defaultRes.view;
        
        WGPUTexelCopyTextureInfo destination = {};
        destination.texture = mDefaultTexture;
        destination.mipLevel = 0;
        destination.origin = {0, 0, 0};
        destination.aspect = WGPUTextureAspect_All;

        WGPUTexelCopyBufferLayout dataLayout = {};
        dataLayout.offset = 0;
        dataLayout.bytesPerRow = 4;
        dataLayout.rowsPerImage = 1;

        WGPUExtent3D writeSize = {1, 1, 1};

        WGPUQueue queue = wgpuDeviceGetQueue(mDevice);
        wgpuQueueWriteTexture(queue, &destination, &blackPixel, 4, &dataLayout, &writeSize);
        wgpuQueueRelease(queue);
    }

    // Default Sampler (Bilinear, Repeat)
    WGPUSamplerDescriptor samplerDesc = {};
    samplerDesc.addressModeU = WGPUAddressMode_Repeat;
    samplerDesc.addressModeV = WGPUAddressMode_Repeat;
    samplerDesc.addressModeW = WGPUAddressMode_Repeat;
    samplerDesc.magFilter = WGPUFilterMode_Linear;
    samplerDesc.minFilter = WGPUFilterMode_Linear;
    samplerDesc.mipmapFilter = WGPUMipmapFilterMode_Linear;
    samplerDesc.lodMinClamp = 0.0f;
    samplerDesc.lodMaxClamp = 32.0f;
    samplerDesc.maxAnisotropy = 1;
    samplerDesc.compare = WGPUCompareFunction_Undefined;

    mDefaultSampler = wgpuDeviceCreateSampler(mDevice, &samplerDesc);
}

bool TextureManager::CreateStaticTexture(const std::string& name, const void* pixels, uint32_t width, uint32_t height, WGPUTextureFormat format) {
    WGPUTextureUsage usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    
    TextureResource res;
    if (!AllocateTextureResource(res, width, height, format, usage)) {
        return false;
    }

    WGPUTexelCopyTextureInfo destination = {};
    destination.texture = res.texture;
    destination.mipLevel = 0;
    destination.origin = {0, 0, 0};
    destination.aspect = WGPUTextureAspect_All;

    // Calculate bytes per row based on format
    uint32_t bytesPerPixel = 4; // Default RGBA8
    if (format == WGPUTextureFormat_RGBA16Float) {
        bytesPerPixel = 8;
    }

    WGPUTexelCopyBufferLayout dataLayout = {};
    dataLayout.offset = 0;
    dataLayout.bytesPerRow = width * bytesPerPixel;
    dataLayout.rowsPerImage = height;

    WGPUExtent3D writeSize = {width, height, 1};

    WGPUQueue queue = wgpuDeviceGetQueue(mDevice);
    wgpuQueueWriteTexture(queue, &destination, pixels, width * height * bytesPerPixel, &dataLayout, &writeSize);
    wgpuQueueRelease(queue);

    // If it already exists, release the old one first
    auto it = mStaticTextures.find(name);
    if (it != mStaticTextures.end()) {
        ReleaseTextureResource(it->second);
    }
    mStaticTextures[name] = res;
    return true;
}

bool TextureManager::CreateRenderTarget(const std::string& name, uint32_t width, uint32_t height, WGPUTextureFormat format, bool pingPong) {
    WGPUTextureUsage usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;

    PingPongTexture ppTex;
    ppTex.isPingPong = pingPong;
    ppTex.writeIndex = 0;

    // If width or height is 0, it's a dynamic render target that resizes with the viewport
    bool isDynamic = (width == 0 || height == 0);
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
        // Set temporary size (will be resized immediately by ResizeRenderTargets)
        width = 1;
        height = 1;
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

    WGPUTextureUsage usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;

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

WGPUTextureView TextureManager::GetReadView(const std::string& name) const {
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

WGPUTextureView TextureManager::GetWriteView(const std::string& name) const {
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

WGPUTextureFormat TextureManager::GetFormat(const std::string& name) const {
    auto itRT = mRenderTargets.find(name);
    if (itRT != mRenderTargets.end()) {
        return itRT->second.resources[0].format;
    }
    auto itStatic = mStaticTextures.find(name);
    if (itStatic != mStaticTextures.end()) {
        return itStatic->second.format;
    }
    return WGPUTextureFormat_Undefined;
}
