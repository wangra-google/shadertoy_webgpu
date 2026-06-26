#include "core/renderer.h"

#include <cmath>
#include <iostream>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Default fallback WGSL shader for Phase 1
const char* DEFAULT_WGSL_SHADER = R"(
struct ShadertoyUniforms {
    iResolution : vec3<f32>,
    iTime       : f32,
    iTimeDelta  : f32,
    iFrameRate  : f32,
    iFrame      : u32,
    _pad0       : u32,
    iMouse      : vec4<f32>,
    iDate       : vec4<f32>,
};

struct CameraUniforms {
    u_ViewProj       : mat4x4<f32>,
    u_Model          : mat4x4<f32>,
    u_NormalMatrix   : mat4x4<f32>,
    u_CameraPosition : vec3<f32>,
    _pad1            : f32,
};

@group(0) @binding(0) var<uniform> u_shadertoy : ShadertoyUniforms;
@group(0) @binding(1) var<uniform> u_camera    : CameraUniforms;

struct VertexInput {
    @location(0) position : vec3<f32>,
    @location(1) normal   : vec3<f32>,
    @location(2) uv       : vec2<f32>,
    @location(3) tangent  : vec4<f32>,
};

struct VertexOutput {
    @builtin(position) position : vec4<f32>,
    @location(0) normal   : vec3<f32>,
    @location(1) uv       : vec2<f32>,
    @location(2) worldPos : vec3<f32>,
};

@vertex
fn vs_main(input : VertexInput) -> VertexOutput {
    var output : VertexOutput;
    let worldPos = u_camera.u_Model * vec4<f32>(input.position, 1.0);
    output.worldPos = worldPos.xyz;
    output.position = u_camera.u_ViewProj * worldPos;
    output.normal = (u_camera.u_NormalMatrix * vec4<f32>(input.normal, 0.0)).xyz;
    output.uv = input.uv;
    return output;
}

@fragment
fn fs_main(input : VertexOutput) -> @location(0) vec4<f32> {
    let N = normalize(input.normal);
    let L = normalize(vec3<f32>(1.0, 2.0, 3.0)); // Simple light source direction
    let diffuse = max(dot(N, L), 0.0) * 0.6 + 0.4;
    
    // Procedural color using uv and time
    let r = input.uv.x * (0.5 + 0.5 * sin(u_shadertoy.iTime));
    let g = input.uv.y * (0.5 + 0.5 * cos(u_shadertoy.iTime));
    let b = 0.5 + 0.5 * sin(u_shadertoy.iTime + 2.0);
    
    let baseColor = vec3<f32>(r, g, b);
    return vec4<f32>(baseColor * diffuse, 1.0);
}
)";

Renderer::Renderer(GraphicsContext* context)
    : m_context(context),
      m_bindGroupLayout(nullptr),
      m_pipelineLayout(nullptr),
      m_renderPipeline(nullptr),
      m_bindGroup(nullptr),
      m_shadertoyUniformBuffer(nullptr),
      m_cameraUniformBuffer(nullptr),
      m_quadMesh(nullptr),
      m_cubeMesh(nullptr),
      m_sphereMesh(nullptr),
      m_geometryType(GeometryType::Quad),
      m_depthTexture(nullptr),
      m_depthTextureView(nullptr) {
    // Clear structs
    memset(&m_shadertoyUniformsVal, 0, sizeof(ShadertoyUniforms));
    memset(&m_cameraUniformsVal, 0, sizeof(CameraUniforms));
}

Renderer::~Renderer() {
    delete m_quadMesh;
    delete m_cubeMesh;
    delete m_sphereMesh;

    if (m_depthTextureView) {
        wgpuTextureViewRelease(m_depthTextureView);
    }
    if (m_depthTexture) {
        wgpuTextureDestroy(m_depthTexture);
        wgpuTextureRelease(m_depthTexture);
    }

    if (m_bindGroup) {
        wgpuBindGroupRelease(m_bindGroup);
    }
    if (m_renderPipeline) {
        wgpuRenderPipelineRelease(m_renderPipeline);
    }
    if (m_pipelineLayout) {
        wgpuPipelineLayoutRelease(m_pipelineLayout);
    }
    if (m_bindGroupLayout) {
        wgpuBindGroupLayoutRelease(m_bindGroupLayout);
    }

    if (m_shadertoyUniformBuffer) {
        wgpuBufferDestroy(m_shadertoyUniformBuffer);
        wgpuBufferRelease(m_shadertoyUniformBuffer);
    }
    if (m_cameraUniformBuffer) {
        wgpuBufferDestroy(m_cameraUniformBuffer);
        wgpuBufferRelease(m_cameraUniformBuffer);
    }
}

bool Renderer::Init() {
    WGPUDevice device = m_context->GetDevice();
    if (!device) {
        std::cerr << "Failed to initialize Renderer: device is null!" << std::endl;
        return false;
    }

    // Create uniform buffers
    WGPUBufferDescriptor uniformBufferDesc = {};
    uniformBufferDesc.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst;
    uniformBufferDesc.mappedAtCreation = false;

    uniformBufferDesc.label = {"Shadertoy Uniform Buffer", WGPU_STRLEN};
    uniformBufferDesc.size = sizeof(ShadertoyUniforms);
    m_shadertoyUniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);

    uniformBufferDesc.label = {"Camera Uniform Buffer", WGPU_STRLEN};
    uniformBufferDesc.size = sizeof(CameraUniforms);
    m_cameraUniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);

    // Create Bind Group Layout
    WGPUBindGroupLayoutEntry bindGroupLayoutEntries[2] = {};

    // Shadertoy Uniforms Entry
    bindGroupLayoutEntries[0].binding = 0;
    bindGroupLayoutEntries[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntries[0].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[0].buffer.minBindingSize = sizeof(ShadertoyUniforms);

    // Camera Uniforms Entry
    bindGroupLayoutEntries[1].binding = 1;
    bindGroupLayoutEntries[1].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    bindGroupLayoutEntries[1].buffer.type = WGPUBufferBindingType_Uniform;
    bindGroupLayoutEntries[1].buffer.hasDynamicOffset = false;
    bindGroupLayoutEntries[1].buffer.minBindingSize = sizeof(CameraUniforms);

    WGPUBindGroupLayoutDescriptor bindGroupLayoutDesc = {};
    bindGroupLayoutDesc.label = {"Renderer Bind Group Layout", WGPU_STRLEN};
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    m_bindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    // Create Bind Group
    WGPUBindGroupEntry bindGroupEntries[2] = {};
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].buffer = m_shadertoyUniformBuffer;
    bindGroupEntries[0].offset = 0;
    bindGroupEntries[0].size = sizeof(ShadertoyUniforms);

    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].buffer = m_cameraUniformBuffer;
    bindGroupEntries[1].offset = 0;
    bindGroupEntries[1].size = sizeof(CameraUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = {"Renderer Bind Group", WGPU_STRLEN};
    bindGroupDesc.layout = m_bindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    m_bindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Create Pipeline Layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = {"Renderer Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &m_bindGroupLayout;
    m_pipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Create Geometries
    m_quadMesh = Mesh::CreateQuad(device);
    m_cubeMesh = Mesh::CreateCube(device);
    m_sphereMesh = Mesh::CreateSphere(device);

    // Depth buffer texture
    CreateDepthTexture();

    // Compile default shader module
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {DEFAULT_WGSL_SHADER, WGPU_STRLEN};
    shaderModuleDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule defaultShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    bool success = CreatePipeline(defaultShaderModule, /*isAsync=*/false);
    wgpuShaderModuleRelease(defaultShaderModule);

    return success;
}

void Renderer::SetUniforms(float time,
                           float time_delta,
                           float frame_rate,
                           int frame,
                           float mouse_x,
                           float mouse_y,
                           float mouse_z,
                           float mouse_w) {
    m_shadertoyUniformsVal.iTime = time;
    m_shadertoyUniformsVal.iTimeDelta = time_delta;
    m_shadertoyUniformsVal.iFrameRate = frame_rate;
    m_shadertoyUniformsVal.iFrame = frame;
    m_shadertoyUniformsVal.iMouse[0] = mouse_x;
    m_shadertoyUniformsVal.iMouse[1] = mouse_y;
    m_shadertoyUniformsVal.iMouse[2] = mouse_z;
    m_shadertoyUniformsVal.iMouse[3] = mouse_w;
}

bool Renderer::UpdateShader(const std::string& wgsl_source) {
    WGPUDevice device = m_context->GetDevice();
    if (!device) {
        return false;
    }

    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {wgsl_source.c_str(), WGPU_STRLEN};
    shaderModuleDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule newShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    bool result = CreatePipeline(newShaderModule, /*isAsync=*/true);
    wgpuShaderModuleRelease(newShaderModule);
    return result;
}

void Renderer::SetGeometryType(GeometryType type) {
    m_geometryType = type;
}

void Renderer::SwapPipeline(WGPURenderPipeline newPipeline) {
    if (m_renderPipeline) {
        wgpuRenderPipelineRelease(m_renderPipeline);
    }
    m_renderPipeline = newPipeline;
}

bool Renderer::CreatePipeline(WGPUShaderModule shaderModule, bool isAsync) {
    WGPUDevice device = m_context->GetDevice();

    // Configure vertex attribute layouts matching Vertex struct
    WGPUVertexAttribute vertexAttributes[4] = {};

    // position (location 0)
    vertexAttributes[0].shaderLocation = 0;
    vertexAttributes[0].format = WGPUVertexFormat_Float32x3;
    vertexAttributes[0].offset = offsetof(Vertex, position);

    // normal (location 1)
    vertexAttributes[1].shaderLocation = 1;
    vertexAttributes[1].format = WGPUVertexFormat_Float32x3;
    vertexAttributes[1].offset = offsetof(Vertex, normal);

    // uv (location 2)
    vertexAttributes[2].shaderLocation = 2;
    vertexAttributes[2].format = WGPUVertexFormat_Float32x2;
    vertexAttributes[2].offset = offsetof(Vertex, uv);

    // tangent (location 3)
    vertexAttributes[3].shaderLocation = 3;
    vertexAttributes[3].format = WGPUVertexFormat_Float32x4;
    vertexAttributes[3].offset = offsetof(Vertex, tangent);

    WGPUVertexBufferLayout vertexBufferLayout = {};
    vertexBufferLayout.arrayStride = sizeof(Vertex);
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.attributeCount = 4;
    vertexBufferLayout.attributes = vertexAttributes;

    // Build color target state
    WGPUColorTargetState colorTarget = {};
    colorTarget.format = m_context->GetSwapChainFormat();
    colorTarget.writeMask = WGPUColorWriteMask_All;
    colorTarget.blend = nullptr;  // standard blending disabled

    WGPUFragmentState fragmentState = {};
    fragmentState.module = shaderModule;
    fragmentState.entryPoint = {"fs_main", WGPU_STRLEN};
    fragmentState.targetCount = 1;
    fragmentState.targets = &colorTarget;

    // Depth Stencil state
    WGPUDepthStencilState depthStencilState = {};
    depthStencilState.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthStencilState.depthWriteEnabled = WGPUOptionalBool_True;
    depthStencilState.depthCompare = WGPUCompareFunction_Less;
    depthStencilState.stencilFront.compare = WGPUCompareFunction_Always;
    depthStencilState.stencilFront.failOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.depthFailOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilFront.passOp = WGPUStencilOperation_Keep;
    depthStencilState.stencilBack = depthStencilState.stencilFront;

    WGPURenderPipelineDescriptor pipelineDesc = {};
    pipelineDesc.label = {"Renderer Render Pipeline", WGPU_STRLEN};
    pipelineDesc.layout = m_pipelineLayout;

    pipelineDesc.vertex.module = shaderModule;
    pipelineDesc.vertex.entryPoint = {"vs_main", WGPU_STRLEN};
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    pipelineDesc.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pipelineDesc.primitive.stripIndexFormat = WGPUIndexFormat_Undefined;
    pipelineDesc.primitive.frontFace = WGPUFrontFace_CCW;
    pipelineDesc.primitive.cullMode = WGPUCullMode_None;  // No face culling

    pipelineDesc.multisample.count = 1;
    pipelineDesc.multisample.mask = ~0u;
    pipelineDesc.multisample.alphaToCoverageEnabled = false;

    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.depthStencil = &depthStencilState;

    if (isAsync) {
        WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo = {};
        callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status,
                                   WGPURenderPipeline pipeline, WGPUStringView message,
                                   void* userdata1, void* userdata2) {
            Renderer* self = static_cast<Renderer*>(userdata1);
            if (status == WGPUCreatePipelineAsyncStatus_Success) {
                self->SwapPipeline(pipeline);
            } else {
                std::cerr << "Asynchronous pipeline compilation failed: ";
                if (message.data && message.length > 0) {
                    std::cerr.write(message.data, message.length);
                } else {
                    std::cerr << "Unknown error";
                }
                std::cerr << std::endl;
            }
        };
        callbackInfo.userdata1 = this;

        wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDesc, callbackInfo);
        return true;
    } else {
        WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        if (!pipeline) {
            std::cerr << "Failed to create render pipeline synchronously!" << std::endl;
            return false;
        }
        SwapPipeline(pipeline);
        return true;
    }
}

void Renderer::UpdateUniformBuffers() {
    WGPUQueue queue = wgpuDeviceGetQueue(m_context->GetDevice());

    // 1. Update Shadertoy Uniforms
    m_shadertoyUniformsVal.iResolution[0] = static_cast<float>(m_context->GetWidth());
    m_shadertoyUniformsVal.iResolution[1] = static_cast<float>(m_context->GetHeight());
    m_shadertoyUniformsVal.iResolution[2] = 1.0f;
    wgpuQueueWriteBuffer(queue, m_shadertoyUniformBuffer, 0, &m_shadertoyUniformsVal,
                         sizeof(ShadertoyUniforms));

    // 2. Update Camera Uniforms
    if (m_geometryType == GeometryType::Quad) {
        // Identity matrices for 2D screen space quad
        Mat4 identity = Mat4::Identity();
        memcpy(m_cameraUniformsVal.u_ViewProj, identity.m, 16 * sizeof(float));
        memcpy(m_cameraUniformsVal.u_Model, identity.m, 16 * sizeof(float));
        memcpy(m_cameraUniformsVal.u_NormalMatrix, identity.m, 16 * sizeof(float));
        m_cameraUniformsVal.u_CameraPosition[0] = 0.0f;
        m_cameraUniformsVal.u_CameraPosition[1] = 0.0f;
        m_cameraUniformsVal.u_CameraPosition[2] = 1.0f;
    } else {
        // 3D camera matrices
        float aspect =
            static_cast<float>(m_context->GetWidth()) / static_cast<float>(m_context->GetHeight());
        Mat4 proj =
            Mat4::Perspective(45.0f * static_cast<float>(M_PI) / 180.0f, aspect, 0.1f, 100.0f);

        Vec3 cameraPos = {0.0f, 1.5f, 3.0f};
        Vec3 cameraTarget = {0.0f, 0.0f, 0.0f};
        Vec3 cameraUp = {0.0f, 1.0f, 0.0f};
        Mat4 view = Mat4::LookAt(cameraPos, cameraTarget, cameraUp);

        Mat4 viewProj = Mat4::Multiply(proj, view);

        // Spin the 3D model over time to show interactivity
        float rotationAngle = m_shadertoyUniformsVal.iTime * 0.5f;
        Mat4 model =
            Mat4::Multiply(Mat4::RotationY(rotationAngle), Mat4::RotationX(rotationAngle * 0.3f));

        Mat4 normalMatrix = Mat4::Transpose(Mat4::Inverse(model));

        memcpy(m_cameraUniformsVal.u_ViewProj, viewProj.m, 16 * sizeof(float));
        memcpy(m_cameraUniformsVal.u_Model, model.m, 16 * sizeof(float));
        memcpy(m_cameraUniformsVal.u_NormalMatrix, normalMatrix.m, 16 * sizeof(float));

        m_cameraUniformsVal.u_CameraPosition[0] = cameraPos.x;
        m_cameraUniformsVal.u_CameraPosition[1] = cameraPos.y;
        m_cameraUniformsVal.u_CameraPosition[2] = cameraPos.z;
    }

    wgpuQueueWriteBuffer(queue, m_cameraUniformBuffer, 0, &m_cameraUniformsVal,
                         sizeof(CameraUniforms));
    wgpuQueueRelease(queue);
}

void Renderer::Render() {
    WGPUDevice device = m_context->GetDevice();
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    UpdateUniformBuffers();

    WGPUTextureView backBufferView = m_context->GetCurrentSwapChainView();
    if (!backBufferView) {
        wgpuQueueRelease(queue);
        return;
    }

    WGPUCommandEncoderDescriptor encoderDesc = {};
    encoderDesc.label = {"Renderer Command Encoder", WGPU_STRLEN};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &encoderDesc);

    WGPURenderPassColorAttachment colorAttachment = {};
    colorAttachment.view = backBufferView;
    colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    colorAttachment.resolveTarget = nullptr;
    colorAttachment.loadOp = WGPULoadOp_Clear;
    colorAttachment.storeOp = WGPUStoreOp_Store;
    colorAttachment.clearValue = {0.1f, 0.1f, 0.12f, 1.0f};

    WGPURenderPassDepthStencilAttachment depthAttachment = {};
    depthAttachment.view = m_depthTextureView;
    depthAttachment.depthLoadOp = WGPULoadOp_Clear;
    depthAttachment.depthStoreOp = WGPUStoreOp_Store;
    depthAttachment.depthClearValue = 1.0f;
    depthAttachment.stencilLoadOp = WGPULoadOp_Clear;
    depthAttachment.stencilStoreOp = WGPUStoreOp_Store;
    depthAttachment.stencilClearValue = 0;

    WGPURenderPassDescriptor renderPassDesc = {};
    renderPassDesc.label = {"Renderer Render Pass", WGPU_STRLEN};
    renderPassDesc.colorAttachmentCount = 1;
    renderPassDesc.colorAttachments = &colorAttachment;
    renderPassDesc.depthStencilAttachment = &depthAttachment;

    WGPURenderPassEncoder renderPass = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

    if (m_renderPipeline) {
        wgpuRenderPassEncoderSetPipeline(renderPass, m_renderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, m_bindGroup, 0, nullptr);

        switch (m_geometryType) {
            case GeometryType::Quad:
                m_quadMesh->Draw(renderPass);
                break;
            case GeometryType::Cube:
                m_cubeMesh->Draw(renderPass);
                break;
            case GeometryType::Sphere:
                m_sphereMesh->Draw(renderPass);
                break;
        }
    }

    wgpuRenderPassEncoderEnd(renderPass);
    wgpuRenderPassEncoderRelease(renderPass);

    WGPUCommandBufferDescriptor cmdBufferDesc = {};
    cmdBufferDesc.label = {"Renderer Command Buffer", WGPU_STRLEN};
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(encoder, &cmdBufferDesc);
    wgpuCommandEncoderRelease(encoder);

    WGPUCommandBuffer commandBuffers[1] = {commandBuffer};
    wgpuQueueSubmit(queue, 1, commandBuffers);

    wgpuCommandBufferRelease(commandBuffer);
    wgpuTextureViewRelease(backBufferView);
    wgpuQueueRelease(queue);

    m_context->Present();
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (m_context->CreateSwapChain(width, height)) {
        CreateDepthTexture();
    }
}

void Renderer::CreateDepthTexture() {
    if (m_depthTextureView) {
        wgpuTextureViewRelease(m_depthTextureView);
        m_depthTextureView = nullptr;
    }
    if (m_depthTexture) {
        wgpuTextureDestroy(m_depthTexture);
        wgpuTextureRelease(m_depthTexture);
        m_depthTexture = nullptr;
    }

    uint32_t width = m_context->GetWidth();
    uint32_t height = m_context->GetHeight();
    if (width == 0) {
        width = 1;
    }
    if (height == 0) {
        height = 1;
    }

    WGPUTextureDescriptor depthDesc = {};
    depthDesc.label = {"Renderer Depth Texture", WGPU_STRLEN};
    depthDesc.usage = WGPUTextureUsage_RenderAttachment;
    depthDesc.dimension = WGPUTextureDimension_2D;
    depthDesc.size = {width, height, 1};
    depthDesc.format = WGPUTextureFormat_Depth24PlusStencil8;
    depthDesc.mipLevelCount = 1;
    depthDesc.sampleCount = 1;
    depthDesc.viewFormatCount = 0;
    depthDesc.viewFormats = nullptr;

    m_depthTexture = wgpuDeviceCreateTexture(m_context->GetDevice(), &depthDesc);
    m_depthTextureView = wgpuTextureCreateView(m_depthTexture, nullptr);
}
