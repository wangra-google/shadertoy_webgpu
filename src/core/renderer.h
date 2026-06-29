#ifndef CORE_RENDERER_H_
#define CORE_RENDERER_H_

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "core/graphics_context.h"
#include "core/math_utils.h"
#include "core/mesh.h"

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
    bool UpdateShader(const std::string& shaderSource,
                      const std::string& language = "wgsl",
                      ShaderStage stage = ShaderStage::Fragment,
                      const std::string& entryPoint = "main",
                      bool isShadertoy = true,
                      std::function<void(bool, const std::string&)> callback = nullptr);
    void SetGeometryType(GeometryType type);
    void Render();
    void Resize(uint32_t width, uint32_t height);

    // Swap compiled pipeline asynchronously (called from static WebGPU callback)
    void SwapPipeline(WGPURenderPipeline newPipeline);

  private:
    bool CreatePipeline(WGPUShaderModule newModule, ShaderStage stage, uint64_t compilationId, bool isAsync,
                        const std::string& vertexEntryPoint, const std::string& fragmentEntryPoint,
                        std::function<void(bool, const std::string&)> callback = nullptr);
    void UpdateUniformBuffers();
    void CreateDepthTexture();

    GraphicsContext* mContext = nullptr;

    // Thread-safe lifetime tracking to prevent Use-After-Free in async WebGPU callbacks
    std::shared_ptr<std::atomic<bool>> mIsAlive;

    // WebGPU Pipeline Objects
    WGPUBindGroupLayout mBindGroupLayout = nullptr;
    WGPUPipelineLayout mPipelineLayout = nullptr;
    WGPURenderPipeline mRenderPipeline = nullptr;
    WGPUBindGroup mBindGroup = nullptr;

    // Uniform Buffers & CPU Structs
    WGPUBuffer mShadertoyUniformBuffer = nullptr;
    WGPUBuffer mCameraUniformBuffer = nullptr;

    ShadertoyUniforms mShadertoyUniformsVal = {};
    CameraUniforms mCameraUniformsVal = {};

    // Procedural Geometries
    std::unique_ptr<Mesh> mQuadMesh = nullptr;
    std::unique_ptr<Mesh> mCubeMesh = nullptr;
    std::unique_ptr<Mesh> mSphereMesh = nullptr;
    GeometryType mGeometryType = GeometryType::Quad;

    // Depth Stencil for 3D meshes
    WGPUTexture mDepthTexture = nullptr;
    WGPUTextureView mDepthTextureView = nullptr;

    // Shader Modules for independent stages
    WGPUShaderModule mVertexShaderModule = nullptr;
    WGPUShaderModule mFragmentShaderModule = nullptr;

    // Entry points for independent stages (helps support GLSL-translated "main" and WGSL "vs_main"/"fs_main")
    std::string mVertexEntryPoint = "vs_main";
    std::string mFragmentEntryPoint = "fs_main";

    // Track compilation requests to prevent race conditions during rapid edits
    uint64_t mLatestRequestedCompilationId = 0;
};

#endif  // CORE_RENDERER_H_
