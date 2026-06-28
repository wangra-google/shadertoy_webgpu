#include "core/renderer.h"

#include <cmath>
#include <iostream>

namespace {
constexpr float kPi = 3.14159265358979323846f;

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
    uViewProj       : mat4x4<f32>,
    uModel          : mat4x4<f32>,
    uNormalMatrix   : mat4x4<f32>,
    uCameraPosition : vec3<f32>,
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
    let worldPos = u_camera.uModel * vec4<f32>(input.position, 1.0);
    output.worldPos = worldPos.xyz;
    output.position = u_camera.uViewProj * worldPos;
    output.normal = (u_camera.uNormalMatrix * vec4<f32>(input.normal, 0.0)).xyz;
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
}  // namespace

Renderer::Renderer(GraphicsContext* context)
    : mContext(context),
      mBindGroupLayout(nullptr),
      mPipelineLayout(nullptr),
      mRenderPipeline(nullptr),
      mBindGroup(nullptr),
      mShadertoyUniformBuffer(nullptr),
      mCameraUniformBuffer(nullptr),
      mQuadMesh(nullptr),
      mCubeMesh(nullptr),
      mSphereMesh(nullptr),
      mGeometryType(GeometryType::Quad),
      mDepthTexture(nullptr),
      mDepthTextureView(nullptr) {
}

Renderer::~Renderer() {
    if (mDepthTextureView) {
        wgpuTextureViewRelease(mDepthTextureView);
    }
    if (mDepthTexture) {
        wgpuTextureDestroy(mDepthTexture);
        wgpuTextureRelease(mDepthTexture);
    }

    if (mBindGroup) {
        wgpuBindGroupRelease(mBindGroup);
    }
    if (mRenderPipeline) {
        wgpuRenderPipelineRelease(mRenderPipeline);
    }
    if (mPipelineLayout) {
        wgpuPipelineLayoutRelease(mPipelineLayout);
    }
    if (mBindGroupLayout) {
        wgpuBindGroupLayoutRelease(mBindGroupLayout);
    }

    if (mShadertoyUniformBuffer) {
        wgpuBufferDestroy(mShadertoyUniformBuffer);
        wgpuBufferRelease(mShadertoyUniformBuffer);
    }
    if (mCameraUniformBuffer) {
        wgpuBufferDestroy(mCameraUniformBuffer);
        wgpuBufferRelease(mCameraUniformBuffer);
    }
}

bool Renderer::Init() {
    WGPUDevice device = mContext->GetDevice();
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
    mShadertoyUniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);

    uniformBufferDesc.label = {"Camera Uniform Buffer", WGPU_STRLEN};
    uniformBufferDesc.size = sizeof(CameraUniforms);
    mCameraUniformBuffer = wgpuDeviceCreateBuffer(device, &uniformBufferDesc);

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
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    // Create Bind Group
    WGPUBindGroupEntry bindGroupEntries[2] = {};
    bindGroupEntries[0].binding = 0;
    bindGroupEntries[0].buffer = mShadertoyUniformBuffer;
    bindGroupEntries[0].offset = 0;
    bindGroupEntries[0].size = sizeof(ShadertoyUniforms);

    bindGroupEntries[1].binding = 1;
    bindGroupEntries[1].buffer = mCameraUniformBuffer;
    bindGroupEntries[1].offset = 0;
    bindGroupEntries[1].size = sizeof(CameraUniforms);

    WGPUBindGroupDescriptor bindGroupDesc = {};
    bindGroupDesc.label = {"Renderer Bind Group", WGPU_STRLEN};
    bindGroupDesc.layout = mBindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    mBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Create Pipeline Layout
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = {"Renderer Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 1;
    pipelineLayoutDesc.bindGroupLayouts = &mBindGroupLayout;
    mPipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Create Geometries
    mQuadMesh = Mesh::CreateQuad(device);
    mCubeMesh = Mesh::CreateCube(device);
    mSphereMesh = Mesh::CreateSphere(device);

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
                           float timeDelta,
                           float frameRate,
                           int frame,
                           float mouseX,
                           float mouseY,
                           float mouseZ,
                           float mouseW) {
    mShadertoyUniformsVal.iTime = time;
    mShadertoyUniformsVal.iTimeDelta = timeDelta;
    mShadertoyUniformsVal.iFrameRate = frameRate;
    mShadertoyUniformsVal.iFrame = frame;
    mShadertoyUniformsVal.iMouse[0] = mouseX;
    mShadertoyUniformsVal.iMouse[1] = mouseY;
    mShadertoyUniformsVal.iMouse[2] = mouseZ;
    mShadertoyUniformsVal.iMouse[3] = mouseW;
}

bool Renderer::UpdateShader(const std::string& wgslSource) {
    WGPUDevice device = mContext->GetDevice();
    if (!device) {
        return false;
    }

    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {wgslSource.c_str(), WGPU_STRLEN};
    shaderModuleDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule newShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    bool result = CreatePipeline(newShaderModule, /*isAsync=*/true);
    wgpuShaderModuleRelease(newShaderModule);
    return result;
}

void Renderer::SetGeometryType(GeometryType type) {
    mGeometryType = type;
}

void Renderer::SwapPipeline(WGPURenderPipeline newPipeline) {
    if (mRenderPipeline) {
        wgpuRenderPipelineRelease(mRenderPipeline);
    }
    mRenderPipeline = newPipeline;
}

bool Renderer::CreatePipeline(WGPUShaderModule shaderModule, bool isAsync) {
    WGPUDevice device = mContext->GetDevice();

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
    colorTarget.format = mContext->GetSwapChainFormat();
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
    pipelineDesc.layout = mPipelineLayout;

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
    WGPUQueue queue = wgpuDeviceGetQueue(mContext->GetDevice());

    // 1. Update Shadertoy Uniforms
    mShadertoyUniformsVal.iResolution[0] = static_cast<float>(mContext->GetWidth());
    mShadertoyUniformsVal.iResolution[1] = static_cast<float>(mContext->GetHeight());
    mShadertoyUniformsVal.iResolution[2] = 1.0f;
    wgpuQueueWriteBuffer(queue, mShadertoyUniformBuffer, 0, &mShadertoyUniformsVal,
                         sizeof(ShadertoyUniforms));

    // 2. Update Camera Uniforms
    if (mGeometryType == GeometryType::Quad) {
        // Identity matrices for 2D screen space quad
        Mat4 identity = Mat4Identity();
        std::memcpy(mCameraUniformsVal.uViewProj, identity.m, 16 * sizeof(float));
        std::memcpy(mCameraUniformsVal.uModel, identity.m, 16 * sizeof(float));
        std::memcpy(mCameraUniformsVal.uNormalMatrix, identity.m, 16 * sizeof(float));
        mCameraUniformsVal.uCameraPosition[0] = 0.0f;
        mCameraUniformsVal.uCameraPosition[1] = 0.0f;
        mCameraUniformsVal.uCameraPosition[2] = 1.0f;
    } else {
        // 3D camera matrices
        float aspect =
            static_cast<float>(mContext->GetWidth()) / static_cast<float>(mContext->GetHeight());
        Mat4 proj =
            Mat4Perspective(45.0f * kPi / 180.0f, aspect, 0.1f, 100.0f);

        Vec3 cameraPos = {0.0f, 1.5f, 3.0f};
        Vec3 cameraTarget = {0.0f, 0.0f, 0.0f};
        Vec3 cameraUp = {0.0f, 1.0f, 0.0f};
        Mat4 view = Mat4LookAt(cameraPos, cameraTarget, cameraUp);

        Mat4 viewProj = Mat4Multiply(proj, view);

        // Spin the 3D model over time to show interactivity
        float rotationAngle = mShadertoyUniformsVal.iTime * 0.5f;
        Mat4 model =
            Mat4Multiply(Mat4RotationY(rotationAngle), Mat4RotationX(rotationAngle * 0.3f));

        Mat4 normalMatrix = Mat4Transpose(Mat4Inverse(model));

        std::memcpy(mCameraUniformsVal.uViewProj, viewProj.m, 16 * sizeof(float));
        std::memcpy(mCameraUniformsVal.uModel, model.m, 16 * sizeof(float));
        std::memcpy(mCameraUniformsVal.uNormalMatrix, normalMatrix.m, 16 * sizeof(float));

        mCameraUniformsVal.uCameraPosition[0] = cameraPos.x;
        mCameraUniformsVal.uCameraPosition[1] = cameraPos.y;
        mCameraUniformsVal.uCameraPosition[2] = cameraPos.z;
    }

    wgpuQueueWriteBuffer(queue, mCameraUniformBuffer, 0, &mCameraUniformsVal,
                         sizeof(CameraUniforms));
    wgpuQueueRelease(queue);
}

void Renderer::Render() {
    WGPUDevice device = mContext->GetDevice();
    WGPUQueue queue = wgpuDeviceGetQueue(device);

    UpdateUniformBuffers();

    WGPUTextureView backBufferView = mContext->GetCurrentSwapChainView();
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
    depthAttachment.view = mDepthTextureView;
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

    if (mRenderPipeline) {
        wgpuRenderPassEncoderSetPipeline(renderPass, mRenderPipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPass, 0, mBindGroup, 0, nullptr);

        switch (mGeometryType) {
            case GeometryType::Quad:
                mQuadMesh->Draw(renderPass);
                break;
            case GeometryType::Cube:
                mCubeMesh->Draw(renderPass);
                break;
            case GeometryType::Sphere:
                mSphereMesh->Draw(renderPass);
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

    mContext->Present();
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (mContext->CreateSwapChain(width, height)) {
        CreateDepthTexture();
    }
}

void Renderer::CreateDepthTexture() {
    if (mDepthTextureView) {
        wgpuTextureViewRelease(mDepthTextureView);
        mDepthTextureView = nullptr;
    }
    if (mDepthTexture) {
        wgpuTextureDestroy(mDepthTexture);
        wgpuTextureRelease(mDepthTexture);
        mDepthTexture = nullptr;
    }

    uint32_t width = mContext->GetWidth();
    uint32_t height = mContext->GetHeight();
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

    mDepthTexture = wgpuDeviceCreateTexture(mContext->GetDevice(), &depthDesc);
    mDepthTextureView = wgpuTextureCreateView(mDepthTexture, nullptr);
}
