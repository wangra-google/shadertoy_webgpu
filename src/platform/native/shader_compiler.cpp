#include "platform/native/shader_compiler.h"

#include <iostream>
#include <vector>

#include <glslang/Public/ShaderLang.h>
#include <glslang/Public/ResourceLimits.h>
#include <SPIRV/GlslangToSpv.h>
#include <tint/tint.h>

#include "core/shadertoy_compat.h"

namespace shader_compiler {

std::string CompileGLSLToWGSL(const std::string& glslSource, ::ShaderStage stage, bool isShadertoy) {
    std::string fullGlsl = glslSource;
    if (isShadertoy) {
        // Shadertoy shaders only define a mainImage() function and use implicit uniforms
        // (like iTime, iResolution) without declaring them. We prepend a compatibility
        // header that declares these uniforms in WebGPU-compatible layouts, maps inputs,
        // and defines a standard main() entry point that calls mainImage().
        fullGlsl = std::string(GLSL_SHADERTOY_HEADER) + glslSource;
    }

    EShLanguage eshStage = EShLangFragment;
    if (stage == ::ShaderStage::Vertex) {
        eshStage = EShLangVertex;
    } else if (stage == ::ShaderStage::Compute) {
        eshStage = EShLangCompute;
    }

    glslang::TShader shader(eshStage);
    const char* shaderStrings[1] = { fullGlsl.c_str() };
    shader.setStrings(shaderStrings, 1);

    // Configure Vulkan/SPIR-V environment.
    // Targeting Vulkan 1.1 and SPIR-V 1.3 to allow modern WGSL/WebGPU feature mappings.
    shader.setEnvInput(glslang::EShSourceGlsl, eshStage, glslang::EShClientVulkan, 110);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    // Use default resource limits provided by glslang library
    const TBuiltInResource* resources = GetDefaultResources();
    if (!resources) {
        std::cerr << "ShaderCompiler: Failed to get default glslang resources!" << std::endl;
        return "";
    }

    EShMessages messages = static_cast<EShMessages>(EShMsgDefault | EShMsgVulkanRules | EShMsgSpvRules);
    if (!shader.parse(resources, 100, false, messages)) {
        std::cerr << "ShaderCompiler: GLSL Parsing Failed!\n" 
                  << "Info Log:\n" << shader.getInfoLog() << "\n"
                  << "Debug Log:\n" << shader.getInfoDebugLog() << std::endl;
        return "";
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages)) {
        std::cerr << "ShaderCompiler: GLSL Linking Failed!\n"
                  << "Info Log:\n" << program.getInfoLog() << "\n"
                  << "Debug Log:\n" << program.getInfoDebugLog() << std::endl;
        return "";
    }

    // Compile to SPIR-V
    std::vector<uint32_t> spirv;
    glslang::SpvOptions spvOptions;
    spvOptions.validate = true;
    glslang::GlslangToSpv(*program.getIntermediate(eshStage), spirv, &spvOptions);

    if (spirv.empty()) {
        std::cerr << "ShaderCompiler: Generated SPIR-V binary is empty!" << std::endl;
        return "";
    }

    // Translate SPIR-V to WGSL using Google Tint.
    // Note: tint::Initialize() and tint::Shutdown() are managed at the application lifecycle level.
    tint::Result<std::string> result = tint::SpirvToWgsl(spirv);

    if (result != tint::Success) {
        std::cerr << "ShaderCompiler: Tint translation from SPIR-V to WGSL failed!\n"
                  << "Reason: " << result.Failure().reason << std::endl;
        return "";
    }

    return result.Get();
}

}  // namespace shader_compiler
