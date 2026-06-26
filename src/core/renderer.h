#ifndef RENDERER_H
#define RENDERER_H

#include <cstddef>
#include <string>
#include <vector>

#include "core/graphics_context.h"
#include "core/math_utils.h"
#include "core/mesh.h"

enum class GeometryType { Quad, Cube, Sphere };

struct ShadertoyUniforms {
    float iResolution[3];  // vec3
    float iTime;
    float iTimeDelta;
    float iFrameRate;
    int32_t iFrame;
    uint32_t _pad0;   // Explicit 4-byte padding to align iMouse to 16-byte boundary
    float iMouse[4];  // vec4
    float iDate[4];   // vec4
};
static_assert(sizeof(ShadertoyUniforms) == 64, "ShadertoyUniforms size must be 64 bytes");
static_assert(offsetof(ShadertoyUniforms, iMouse) == 32,
              "ShadertoyUniforms.iMouse offset must be 32 bytes");

struct CameraUniforms {
    float u_ViewProj[16];       // mat4x4
    float u_Model[16];          // mat4x4
    float u_NormalMatrix[16];   // mat4x4
    float u_CameraPosition[3];  // vec3
    float padding;              // Explicit 4-byte padding to align struct to 16-bytes
};
static_assert(sizeof(CameraUniforms) == 208, "CameraUniforms size must be 208 bytes");
static_assert(offsetof(CameraUniforms, u_CameraPosition) == 192,
              "CameraUniforms.u_CameraPosition offset must be 192 bytes");

class Renderer {
  public:
    Renderer(GraphicsContext* context);
    ~Renderer();

    bool Init();
    void SetUniforms(float time,
                     float time_delta,
                     float frame_rate,
                     int frame,
                     float mouse_x,
                     float mouse_y,
                     float mouse_z,
                     float mouse_w);
    bool UpdateShader(const std::string& wgsl_source);
    void SetGeometryType(GeometryType type);
    void Render();
    void Resize(uint32_t width, uint32_t height);

    // Swap compiled pipeline asynchronously (called from static WebGPU callback)
    void SwapPipeline(WGPURenderPipeline newPipeline);

  private:
    bool CreatePipeline(WGPUShaderModule shaderModule, bool isAsync);
    void UpdateUniformBuffers();

    GraphicsContext* m_context = nullptr;

    // WebGPU Pipeline Objects
    WGPUBindGroupLayout m_bindGroupLayout = nullptr;
    WGPUPipelineLayout m_pipelineLayout = nullptr;
    WGPURenderPipeline m_renderPipeline = nullptr;
    WGPUBindGroup m_bindGroup = nullptr;

    // Uniform Buffers & CPU Structs
    WGPUBuffer m_shadertoyUniformBuffer = nullptr;
    WGPUBuffer m_cameraUniformBuffer = nullptr;

    ShadertoyUniforms m_shadertoyUniformsVal = {};
    CameraUniforms m_cameraUniformsVal = {};

    // Procedural Geometries
    Mesh* m_quadMesh = nullptr;
    Mesh* m_cubeMesh = nullptr;
    Mesh* m_sphereMesh = nullptr;
    GeometryType m_geometryType = GeometryType::Quad;

    // Depth Stencil for 3D meshes
    WGPUTexture m_depthTexture = nullptr;
    WGPUTextureView m_depthTextureView = nullptr;
    void CreateDepthTexture();
};

#endif  // RENDERER_H
