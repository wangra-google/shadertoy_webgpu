#include "core/renderer.h"

#include <cmath>
#include <iostream>

#if !defined(__EMSCRIPTEN__)
#include "platform/native/shader_compiler.h"
#endif

namespace {
constexpr float kPi = 3.14159265358979323846f;

// Default fallback WGSL shader for Phase 1
const char* DEFAULT_WGSL_SHADER = R"(
struct ShadertoyUniforms {
    iResolution : vec3<f32>,
    iTime       : f32,
    iTimeDelta  : f32,
    iFrameRate  : f32,
    iFrame      : i32,
    pad0        : u32,
    iMouse      : vec4<f32>,
    iDate       : vec4<f32>,
};

struct CameraUniforms {
    uViewProj       : mat4x4<f32>,
    uModel          : mat4x4<f32>,
    uNormalMatrix   : mat4x4<f32>,
    uCameraPosition : vec3<f32>,
    padding         : f32,
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
      mIsAlive(std::make_shared<std::atomic<bool>>(true)) {}

Renderer::~Renderer() {
    mIsAlive->store(false);
    
    // Release passes resources
    for (auto& pass : mPasses) {
        if (pass.bindGroup1) {
            wgpuBindGroupRelease(pass.bindGroup1);
        }
        if (pass.vertexShaderModule) {
            wgpuShaderModuleRelease(pass.vertexShaderModule);
        }
        if (pass.fragmentShaderModule) {
            wgpuShaderModuleRelease(pass.fragmentShaderModule);
        }
        if (pass.pipeline) {
            wgpuRenderPipelineRelease(pass.pipeline);
        }
    }
    mPasses.clear();

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
    if (mPipelineLayout) {
        wgpuPipelineLayoutRelease(mPipelineLayout);
    }
    if (mBindGroupLayout) {
        wgpuBindGroupLayoutRelease(mBindGroupLayout);
    }
    if (mBindGroupLayout1) {
        wgpuBindGroupLayoutRelease(mBindGroupLayout1);
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

    // Initialize Texture Manager
    mTextureManager = std::make_unique<TextureManager>(device);

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

    // Create Bind Group Layout 0 (Uniforms)
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
    bindGroupLayoutDesc.label = {"Renderer Bind Group Layout 0 (Uniforms)", WGPU_STRLEN};
    bindGroupLayoutDesc.entryCount = 2;
    bindGroupLayoutDesc.entries = bindGroupLayoutEntries;
    mBindGroupLayout = wgpuDeviceCreateBindGroupLayout(device, &bindGroupLayoutDesc);

    // Create Bind Group Layout 1 (Textures & Sampler)
    WGPUBindGroupLayoutEntry textureLayoutEntries[5] = {};
    
    // Sampler (Binding 0)
    textureLayoutEntries[0].binding = 0;
    textureLayoutEntries[0].visibility = WGPUShaderStage_Fragment;
    textureLayoutEntries[0].sampler.type = WGPUSamplerBindingType_Filtering;

    // iChannel0..3 (Bindings 1..4)
    for (int i = 0; i < 4; ++i) {
        textureLayoutEntries[i + 1].binding = i + 1;
        textureLayoutEntries[i + 1].visibility = WGPUShaderStage_Fragment;
        textureLayoutEntries[i + 1].texture.sampleType = WGPUTextureSampleType_Float;
        textureLayoutEntries[i + 1].texture.viewDimension = WGPUTextureViewDimension_2D;
        textureLayoutEntries[i + 1].texture.multisampled = false;
    }

    WGPUBindGroupLayoutDescriptor textureLayoutDesc = {};
    textureLayoutDesc.label = {"Renderer Bind Group Layout 1 (Textures)", WGPU_STRLEN};
    textureLayoutDesc.entryCount = 5;
    textureLayoutDesc.entries = textureLayoutEntries;
    mBindGroupLayout1 = wgpuDeviceCreateBindGroupLayout(device, &textureLayoutDesc);

    // Create Bind Group 0 (Uniforms)
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
    bindGroupDesc.label = {"Renderer Bind Group 0 (Uniforms)", WGPU_STRLEN};
    bindGroupDesc.layout = mBindGroupLayout;
    bindGroupDesc.entryCount = 2;
    bindGroupDesc.entries = bindGroupEntries;
    mBindGroup = wgpuDeviceCreateBindGroup(device, &bindGroupDesc);

    // Create Pipeline Layout
    WGPUBindGroupLayout layouts[2] = {mBindGroupLayout, mBindGroupLayout1};
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {};
    pipelineLayoutDesc.label = {"Renderer Pipeline Layout", WGPU_STRLEN};
    pipelineLayoutDesc.bindGroupLayoutCount = 2;
    pipelineLayoutDesc.bindGroupLayouts = layouts;
    mPipelineLayout = wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);

    // Create Geometries
    mQuadMesh = Mesh::CreateQuad(device);
    mCubeMesh = Mesh::CreateCube(device);
    mSphereMesh = Mesh::CreateSphere(device);

    // Depth buffer texture
    CreateDepthTexture();

    // Create default render targets for Buffer A, B, C, D
    mTextureManager->CreateRenderTarget("BufferA", 0, 0, WGPUTextureFormat_RGBA8Unorm, true);
    mTextureManager->CreateRenderTarget("BufferB", 0, 0, WGPUTextureFormat_RGBA8Unorm, true);
    mTextureManager->CreateRenderTarget("BufferC", 0, 0, WGPUTextureFormat_RGBA8Unorm, true);
    mTextureManager->CreateRenderTarget("BufferD", 0, 0, WGPUTextureFormat_RGBA8Unorm, true);

    // Resize render targets to match initial viewport size
    mTextureManager->ResizeRenderTargets(mContext->GetWidth(), mContext->GetHeight());

    // Initialize passes (BufferA, BufferB, BufferC, BufferD, Image)
    mPasses.push_back({"BufferA", false});
    mPasses.push_back({"BufferB", false});
    mPasses.push_back({"BufferC", false});
    mPasses.push_back({"BufferD", false});
    mPasses.push_back({"Image", true}); // Image is enabled by default

    // Compile default shader module for the Image pass
    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {DEFAULT_WGSL_SHADER, WGPU_STRLEN};
    shaderModuleDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule defaultShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    if (!defaultShaderModule) {
        std::cerr << "Failed to create default shader module!" << std::endl;
        return false;
    }

    for (auto& pass : mPasses) {
        pass.vertexShaderModule = defaultShaderModule;
        wgpuShaderModuleAddRef(defaultShaderModule);
    }

    RenderPass* imagePass = FindPass("Image");
    imagePass->fragmentShaderModule = defaultShaderModule;
    wgpuShaderModuleAddRef(defaultShaderModule);

    bool success = CreatePipeline(*imagePass, defaultShaderModule, ShaderStage::Fragment, 0, /*isAsync=*/false, "vs_main", "fs_main");

    wgpuShaderModuleRelease(defaultShaderModule);
    return success;
}

Renderer::RenderPass* Renderer::FindPass(const std::string& name) {
    for (auto& pass : mPasses) {
        if (pass.name == name) {
            return &pass;
        }
    }
    return nullptr;
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

bool Renderer::UpdateShader(const std::string& passName,
                            const std::string& shaderSource,
                            const std::string& language,
                            ShaderStage stage,
                            const std::string& entryPoint,
                            bool isShadertoy,
                            std::function<void(bool, const std::string&)> callback) {
    if (stage == ShaderStage::Compute) {
        std::cerr << "Renderer: Compute shaders are not supported in the current rendering pipeline (Phase 4)." << std::endl;
        if (callback) {
            callback(false, "Compute shaders are not supported in the current rendering pipeline.");
        }
        return false;
    }

    RenderPass* pass = FindPass(passName);
    if (!pass) {
        std::cerr << "Renderer: Could not find pass: " << passName << std::endl;
        if (callback) {
            callback(false, "Pass not found.");
        }
        return false;
    }

    WGPUDevice device = mContext->GetDevice();
    if (!device) {
        if (callback) {
            callback(false, "WebGPU device is not initialized.");
        }
        return false;
    }

    pass->latestRequestedCompilationId++;
    uint64_t compilationId = pass->latestRequestedCompilationId;

    std::string wgslSource;
    if (language == "glsl") {
#if defined(__EMSCRIPTEN__)
        std::cerr << "Renderer: GLSL compilation is not supported in the WebAssembly core. "
                  << "Please compile to WGSL in JavaScript." << std::endl;
        if (callback) {
            callback(false, "GLSL compilation is not supported in the WebAssembly core.");
        }
        return false;
#else
        // Compile GLSL to WGSL on native desktop.
        wgslSource = shader_compiler::CompileGLSLToWGSL(shaderSource, stage, isShadertoy);
        if (wgslSource.empty()) {
            std::cerr << "Renderer: Failed to compile GLSL to WGSL!" << std::endl;
            if (callback) {
                callback(false, "Failed to compile GLSL to WGSL.");
            }
            return false;
        }
#endif
    } else {
        wgslSource = shaderSource;
    }

    WGPUShaderModuleDescriptor shaderModuleDesc = {};
    WGPUShaderSourceWGSL wgslDesc = {};
    wgslDesc.chain.next = nullptr;
    wgslDesc.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgslDesc.code = {wgslSource.c_str(), WGPU_STRLEN};
    shaderModuleDesc.nextInChain = &wgslDesc.chain;

    WGPUShaderModule newShaderModule = wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
    std::string vertexEP = (stage == ShaderStage::Vertex) ? entryPoint : pass->vertexEntryPoint;
    std::string fragmentEP = (stage == ShaderStage::Fragment) ? entryPoint : pass->fragmentEntryPoint;

    // Enable the pass when we successfully start compilation for it
    if (passName != "Image") {
        pass->enabled = true;
    }

    bool result = CreatePipeline(*pass, newShaderModule, stage, compilationId, /*isAsync=*/true,
                                 vertexEP, fragmentEP, callback);
    wgpuShaderModuleRelease(newShaderModule);
    return result;
}

void Renderer::SetGeometryType(GeometryType type) {
    mGeometryType = type;
}

void Renderer::SetPassInput(const std::string& passName, int channel, const std::string& inputName) {
    RenderPass* pass = FindPass(passName);
    if (pass && channel >= 0 && channel < 4) {
        pass->inputs[channel] = inputName;
    }
}

void Renderer::SetPassEnabled(const std::string& passName, bool enabled) {
    RenderPass* pass = FindPass(passName);
    if (pass) {
        pass->enabled = enabled;
    }
}

void Renderer::LoadTextureData(const std::string& name, const void* pixels, uint32_t width, uint32_t height, const std::string& format) {
    if (!mTextureManager) return;
    
    WGPUTextureFormat wgpuFormat = WGPUTextureFormat_RGBA8Unorm;
    if (format == "srgb") {
        wgpuFormat = WGPUTextureFormat_RGBA8UnormSrgb;
    } else if (format == "hdr") {
        wgpuFormat = WGPUTextureFormat_RGBA16Float;
    }

    mTextureManager->CreateStaticTexture(name, pixels, width, height, wgpuFormat);
}

void Renderer::SwapPipeline(const std::string& passName, WGPURenderPipeline newPipeline, WGPUShaderModule newModule, ShaderStage stage, const std::string& entryPoint) {
    RenderPass* pass = FindPass(passName);
    if (!pass) return;

    if (pass->pipeline) {
        wgpuRenderPipelineRelease(pass->pipeline);
    }
    pass->pipeline = newPipeline;

    if (stage == ShaderStage::Fragment) {
        if (pass->fragmentShaderModule) {
            wgpuShaderModuleRelease(pass->fragmentShaderModule);
        }
        pass->fragmentShaderModule = newModule;
        wgpuShaderModuleAddRef(newModule);
        pass->fragmentEntryPoint = entryPoint;
    } else if (stage == ShaderStage::Vertex) {
        if (pass->vertexShaderModule) {
            wgpuShaderModuleRelease(pass->vertexShaderModule);
        }
        pass->vertexShaderModule = newModule;
        wgpuShaderModuleAddRef(newModule);
        pass->vertexEntryPoint = entryPoint;
    }
}

struct PipelineAsyncData {
    Renderer* renderer;
    std::string passName;
    WGPUShaderModule shaderModule;
    ShaderStage stage;
    uint64_t compilationId;
    std::string vertexEntryPoint;
    std::string fragmentEntryPoint;
    std::shared_ptr<std::atomic<bool>> isAlive;
    std::function<void(bool, const std::string&)> callback;
};

bool Renderer::CreatePipeline(RenderPass& pass, WGPUShaderModule newModule, ShaderStage stage, uint64_t compilationId, bool isAsync,
                              const std::string& vertexEntryPoint, const std::string& fragmentEntryPoint,
                              std::function<void(bool, const std::string&)> callback) {
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
    if (pass.name == "Image") {
        colorTarget.format = mContext->GetSwapChainFormat();
    } else {
        // Query the actual format from the texture manager to support HDR/sRGB targets
        WGPUTextureFormat format = mTextureManager->GetFormat(pass.name);
        if (format == WGPUTextureFormat_Undefined) {
            format = WGPUTextureFormat_RGBA8Unorm;
        }
        colorTarget.format = format;
    }
    colorTarget.writeMask = WGPUColorWriteMask_All;
    colorTarget.blend = nullptr;  // standard blending disabled

    WGPUFragmentState fragmentState = {};
    fragmentState.entryPoint = {fragmentEntryPoint.c_str(), WGPU_STRLEN};
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
    pipelineDesc.label = { (pass.name + " Render Pipeline").c_str(), WGPU_STRLEN };
    pipelineDesc.layout = mPipelineLayout;

    pipelineDesc.vertex.entryPoint = {vertexEntryPoint.c_str(), WGPU_STRLEN};
    pipelineDesc.vertex.bufferCount = 1;
    pipelineDesc.vertex.buffers = &vertexBufferLayout;

    // Bind the shader modules dynamically.
    if (stage == ShaderStage::Fragment) {
        pipelineDesc.vertex.module = pass.vertexShaderModule;
        fragmentState.module = newModule;
    } else {
        pipelineDesc.vertex.module = newModule;
        fragmentState.module = pass.fragmentShaderModule;
    }

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
        wgpuShaderModuleAddRef(newModule);

        auto* asyncData = new PipelineAsyncData{
            this, pass.name, newModule, stage, compilationId, vertexEntryPoint, fragmentEntryPoint, mIsAlive, callback};

        WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo = {};
#ifdef __EMSCRIPTEN__
        // On Web, JS is single-threaded so AllowSpontaneous is safe and doesn't require manual event ticking.
        callbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
#else
        // On Native, use AllowProcessEvents to run callbacks on the main thread and avoid background thread races.
        callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
        callbackInfo.callback = [](WGPUCreatePipelineAsyncStatus status,
                                   WGPURenderPipeline pipeline, WGPUStringView message,
                                   void* userdata1, void* userdata2) {
            auto* data = static_cast<PipelineAsyncData*>(userdata1);
            
            if (!data->isAlive->load()) {
                wgpuShaderModuleRelease(data->shaderModule);
                if (status == WGPUCreatePipelineAsyncStatus_Success) {
                    wgpuRenderPipelineRelease(pipeline);
                }
                delete data;
                return;
            }

            Renderer* self = data->renderer;
            WGPUShaderModule newModule = data->shaderModule;
            ShaderStage stage = data->stage;

            auto* passPtr = self->FindPass(data->passName);
            if (!passPtr) {
                wgpuShaderModuleRelease(newModule);
                if (status == WGPUCreatePipelineAsyncStatus_Success) {
                    wgpuRenderPipelineRelease(pipeline);
                }
                delete data;
                return;
            }

            if (data->compilationId < passPtr->latestRequestedCompilationId) {
                std::cout << "Renderer: Discarding obsolete compilation for pass " << data->passName
                          << " (ID: " << data->compilationId << ", Latest: " 
                          << passPtr->latestRequestedCompilationId << ")" << std::endl;
                if (status == WGPUCreatePipelineAsyncStatus_Success) {
                    wgpuRenderPipelineRelease(pipeline);
                }
                wgpuShaderModuleRelease(newModule);
                if (data->callback) {
                    data->callback(false, "Obsolete compilation discarded.");
                }
                delete data;
                return;
            }

            bool success = false;
            std::string errorMsg = "";

            if (status == WGPUCreatePipelineAsyncStatus_Success) {
                self->SwapPipeline(data->passName, pipeline, newModule, stage, 
                                   (stage == ShaderStage::Fragment) ? data->fragmentEntryPoint : data->vertexEntryPoint);
                success = true;
            } else {
                std::cerr << "Asynchronous pipeline compilation failed for pass " << data->passName << ": ";
                if (message.data && message.length > 0) {
                    std::cerr.write(message.data, message.length);
                    errorMsg = std::string(message.data, message.length);
                } else {
                    std::cerr << "Unknown error";
                    errorMsg = "Unknown error";
                }
                std::cerr << std::endl;
            }

            if (data->callback) {
                data->callback(success, errorMsg);
            }

            wgpuShaderModuleRelease(newModule); // Release the reference held by asyncData (fixes ref leak)
            delete data;
        };
        callbackInfo.userdata1 = asyncData;

        wgpuDeviceCreateRenderPipelineAsync(device, &pipelineDesc, callbackInfo);
        return true;
    } else {
        WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(device, &pipelineDesc);
        if (!pipeline) {
            std::cerr << "Failed to create render pipeline synchronously for pass " << pass.name << "!" << std::endl;
            if (callback) {
                callback(false, "Failed to create render pipeline synchronously.");
            }
            return false;
        }
        SwapPipeline(pass.name, pipeline, newModule, stage, (stage == ShaderStage::Fragment) ? fragmentEntryPoint : vertexEntryPoint);
        if (callback) {
            callback(true, "");
        }
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

WGPUBindGroup Renderer::GetOrCreateBindGroup1(RenderPass& pass) {
    WGPUTextureView currentViews[4] = {};
    bool cacheValid = (pass.bindGroup1 != nullptr);

    for (int i = 0; i < 4; ++i) {
        WGPUTextureView view = nullptr;
        if (!pass.inputs[i].empty()) {
            view = mTextureManager->GetReadView(pass.inputs[i]);
        }
        if (!view) {
            view = mTextureManager->GetDefaultView();
        }
        currentViews[i] = view;
        if (view != pass.boundViews[i]) {
            cacheValid = false;
        }
    }

    if (cacheValid) {
        return pass.bindGroup1;
    }

    // Recreate bind group
    if (pass.bindGroup1) {
        wgpuBindGroupRelease(pass.bindGroup1);
        pass.bindGroup1 = nullptr;
    }

    WGPUDevice device = mContext->GetDevice();
    WGPUBindGroupEntry entries[5] = {};
    
    // Sampler
    entries[0].binding = 0;
    entries[0].sampler = mTextureManager->GetDefaultSampler();

    // iChannel0..3
    for (int i = 0; i < 4; ++i) {
        entries[i + 1].binding = i + 1;
        entries[i + 1].textureView = currentViews[i];
        pass.boundViews[i] = currentViews[i];
    }

    WGPUBindGroupDescriptor desc = {};
    desc.label = { (pass.name + " Bind Group 1 (Textures)").c_str(), WGPU_STRLEN };
    desc.layout = mBindGroupLayout1;
    desc.entryCount = 5;
    desc.entries = entries;

    pass.bindGroup1 = wgpuDeviceCreateBindGroup(device, &desc);
    return pass.bindGroup1;
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

    // Execute each pass in sequence
    for (auto& pass : mPasses) {
        if (!pass.enabled || !pass.pipeline) {
            continue;
        }

        WGPURenderPassColorAttachment colorAttachment = {};
        if (pass.name == "Image") {
            colorAttachment.view = backBufferView;
        } else {
            colorAttachment.view = mTextureManager->GetWriteView(pass.name);
        }
        
        if (!colorAttachment.view) {
            continue;
        }

        colorAttachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
        colorAttachment.resolveTarget = nullptr;
        colorAttachment.loadOp = WGPULoadOp_Clear;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = {0.0f, 0.0f, 0.0f, 1.0f};

        WGPURenderPassDepthStencilAttachment depthAttachment = {};
        depthAttachment.view = mDepthTextureView;
        depthAttachment.depthLoadOp = WGPULoadOp_Clear;
        depthAttachment.depthStoreOp = WGPUStoreOp_Store;
        depthAttachment.depthClearValue = 1.0f;
        depthAttachment.stencilLoadOp = WGPULoadOp_Clear;
        depthAttachment.stencilStoreOp = WGPUStoreOp_Store;
        depthAttachment.stencilClearValue = 0;

        WGPURenderPassDescriptor renderPassDesc = {};
        renderPassDesc.label = { (pass.name + " Render Pass").c_str(), WGPU_STRLEN };
        renderPassDesc.colorAttachmentCount = 1;
        renderPassDesc.colorAttachments = &colorAttachment;
        renderPassDesc.depthStencilAttachment = &depthAttachment;

        WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(encoder, &renderPassDesc);

        // Get or create the cached bind group 1 for textures
        WGPUBindGroup bindGroup1 = GetOrCreateBindGroup1(pass);

        wgpuRenderPassEncoderSetPipeline(renderPassEncoder, pass.pipeline);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 0, mBindGroup, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(renderPassEncoder, 1, bindGroup1, 0, nullptr);

        switch (mGeometryType) {
            case GeometryType::Quad:
                mQuadMesh->Draw(renderPassEncoder);
                break;
            case GeometryType::Cube:
                mCubeMesh->Draw(renderPassEncoder);
                break;
            case GeometryType::Sphere:
                mSphereMesh->Draw(renderPassEncoder);
                break;
        }

        wgpuRenderPassEncoderEnd(renderPassEncoder);
        wgpuRenderPassEncoderRelease(renderPassEncoder);

        // Swap ping-pong textures at the end of the pass
        if (pass.name != "Image") {
            mTextureManager->SwapPingPong(pass.name);
        }
    }

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
    if (!mContext) return;
    if (mContext->CreateSwapChain(width, height)) {
        CreateDepthTexture();
        if (mTextureManager) {
            mTextureManager->ResizeRenderTargets(width, height);
        }
    }
}

void Renderer::CreateDepthTexture() {
    if (!mContext) return;
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
