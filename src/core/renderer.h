#ifndef CORE_RENDERER_H_
#define CORE_RENDERER_H_
#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <webgpu/webgpu_cpp.h>
#include "core/graphics_context.h"
#include "core/math_utils.h"
#include "core/mesh.h"
#include "core/texture_manager.h"

enum class GeometryType { Quad, Cube, Sphere };

enum class ShaderStage { Vertex, Fragment, Compute };

struct ShadertoyUniforms {
    float iResolution[3];  // vec3
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    int32_t iFrame;
    uint32_t pad0;   // Explicit 4-byte padding to align iMouse to 16-byte boundary
    float iMouse[4];  // vec4
    float iDate[4];   // vec4
};
static_assert(sizeof(ShadertoyUniforms) == 64, "ShadertoyUniforms size must be 64 bytes");
static_assert(offsetof(ShadertoyUniforms, iMouse) == 32,
              "ShadertoyUniforms.iMouse offset must be 32 bytes");

struct CameraUniforms {
    float uViewProj[16];       // mat4x4
    float uModel[16];          // mat4x4
    float uNormalMatrix[16];   // mat4x4
    float uCameraPosition[3];  // vec3
    float padding;              // Explicit 4-byte padding to align struct to 16-bytes
};
static_assert(sizeof(CameraUniforms) == 208, "CameraUniforms size must be 208 bytes");
static_assert(offsetof(CameraUniforms, uCameraPosition) == 192,
              "CameraUniforms.uCameraPosition offset must be 192 bytes");

class Renderer {
  public:
    struct RenderPass {
        std::string name;
        bool enabled = false;
        
        // WebGPU Pipeline Objects
        wgpu::ShaderModule vertexShaderModule = nullptr;
        wgpu::ShaderModule fragmentShaderModule = nullptr;
        wgpu::RenderPipeline pipeline = nullptr;
        
        // Shader entry points
        std::string vertexEntryPoint = "vs_main";
        std::string fragmentEntryPoint = "fs_main";
        
        // Input texture names for iChannel0..3
        std::array<std::string, 4> inputs = {};
        
        // Track compilation requests to prevent race conditions
        uint64_t latestRequestedCompilationId = 0;

        // Cached Bind Group 1 & bound texture views
        wgpu::BindGroup bindGroup1 = nullptr;
        std::array<wgpu::TextureView, 4> boundViews = {};
    };

    explicit Renderer(GraphicsContext* context);
    ~Renderer();

    bool Init();
    void SetUniforms(float time,
                     float timeDelta,
                     float frameRate,
                     int frame,
                     float mouseX,
                     float mouseY,
                     float mouseZ,
                     float mouseW);
    bool UpdateShader(const std::string& passName,
                      const std::string& shaderSource,
                      const std::string& language = "wgsl",
                      ShaderStage stage = ShaderStage::Fragment,
                      const std::string& entryPoint = "main",
                      bool isShadertoy = true,
                      std::function<void(bool, const std::string&)> callback = nullptr);
    void SetGeometryType(GeometryType type);
    void Render();
    void Resize(uint32_t width, uint32_t height);

    // Set input channel source for a specific pass
    void SetPassInput(const std::string& passName, int channel, const std::string& inputName);

    // Enable or disable a specific pass
    void SetPassEnabled(const std::string& passName, bool enabled);

    // Load texture data from CPU memory
    void LoadTextureData(const std::string& name, const void* pixels, uint32_t width, uint32_t height, const std::string& format);

    // Swap compiled pipeline asynchronously for a specific pass (called from WebGPU callback)
    void SwapPipeline(const std::string& passName, wgpu::RenderPipeline newPipeline, wgpu::ShaderModule newModule, ShaderStage stage, const std::string& entryPoint);

    // Get the texture manager
    TextureManager* GetTextureManager() const { return mTextureManager.get(); }

  private:
    RenderPass* FindPass(const std::string& name);
    bool CreatePipeline(RenderPass& pass, wgpu::ShaderModule newModule, ShaderStage stage, uint64_t compilationId, bool isAsync,
                        const std::string& vertexEntryPoint, const std::string& fragmentEntryPoint,
                        std::function<void(bool, const std::string&)> callback = nullptr);
    void UpdateUniformBuffers();
    void CreateDepthTexture();
    wgpu::BindGroup GetOrCreateBindGroup1(RenderPass& pass);

    GraphicsContext* mContext = nullptr;

    // Thread-safe lifetime tracking to prevent Use-After-Free in async WebGPU callbacks
    std::shared_ptr<std::atomic<bool>> mIsAlive;

    // Texture Manager
    std::unique_ptr<TextureManager> mTextureManager = nullptr;

    // WebGPU Pipeline Layout Objects
    wgpu::BindGroupLayout mBindGroupLayout = nullptr;  // Group 0: Uniforms
    wgpu::BindGroupLayout mBindGroupLayout1 = nullptr; // Group 1: Textures
    wgpu::PipelineLayout mPipelineLayout = nullptr;
    wgpu::BindGroup mBindGroup = nullptr;             // Group 0 Bind Group (Shared)

    // Uniform Buffers & CPU Structs
    wgpu::Buffer mShadertoyUniformBuffer = nullptr;
    wgpu::Buffer mCameraUniformBuffer = nullptr;

    ShadertoyUniforms mShadertoyUniformsVal = {};
    CameraUniforms mCameraUniformsVal = {};

    // Procedural Geometries
    std::unique_ptr<Mesh> mQuadMesh = nullptr;
    std::unique_ptr<Mesh> mCubeMesh = nullptr;
    std::unique_ptr<Mesh> mSphereMesh = nullptr;
    GeometryType mGeometryType = GeometryType::Quad;

    // Depth Stencil (Shared across passes)
    wgpu::Texture mDepthTexture = nullptr;
    wgpu::TextureView mDepthTextureView = nullptr;

    // Render passes (BufferA, BufferB, BufferC, BufferD, Image)
    std::vector<RenderPass> mPasses;
};

#endif  // CORE_RENDERER_H_

