#ifndef PLATFORM_NATIVE_SHADER_COMPILER_H_
#define PLATFORM_NATIVE_SHADER_COMPILER_H_

#include <string>

#include "core/renderer.h"

namespace shader_compiler {

// Compiles a GLSL shader string into a WGSL shader string.
// Prepends the Shadertoy compatibility header if isShadertoy is true.
// Returns the compiled WGSL string, or an empty string on compilation failure.
std::string CompileGLSLToWGSL(const std::string& glslSource, ::ShaderStage stage, bool isShadertoy = false);

}  // namespace shader_compiler

#endif  // PLATFORM_NATIVE_SHADER_COMPILER_H_
