#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <emscripten.h>

#include <iostream>
#include <memory>
#include <string>

#include "core/renderer.h"
#include "core/shadertoy_compat.h"
#include "platform/web/web_context.h"

class EngineCore {
  public:
    explicit EngineCore(const std::string& canvasSelector)
        : mCanvasSelector(canvasSelector), mContext(nullptr), mRenderer(nullptr) {}

    ~EngineCore() = default;

    bool Initialize(int width, int height) {
        mContext = std::make_unique<WebContext>(mCanvasSelector, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        if (!mContext->Init()) {
            std::cerr << "EngineCore: WebContext Init failed!" << std::endl;
            return false;
        }

        mRenderer = std::make_unique<Renderer>(mContext.get());
        if (!mRenderer->Init()) {
            std::cerr << "EngineCore: Renderer Init failed!" << std::endl;
            return false;
        }

        return true;
    }

    bool UpdateShader(const std::string& passName,
                      const std::string& shaderSource,
                      const std::string& language,
                      const std::string& stage,
                      const std::string& entryPoint,
                      bool isShadertoy,
                      emscripten::val callback) {
        if (!mRenderer) {
            if (!callback.isUndefined() && !callback.isNull()) {
                callback(false, std::string("Renderer is not initialized."));
            }
            return false;
        }
        ShaderStage cStage = ShaderStage::Fragment;
        if (stage == "vertex") {
            cStage = ShaderStage::Vertex;
        } else if (stage == "compute") {
            cStage = ShaderStage::Compute;
        }

        std::function<void(bool, const std::string&)> cppCallback = nullptr;
        if (!callback.isUndefined() && !callback.isNull()) {
            cppCallback = [callback](bool success, const std::string& errorMsg) {
                callback(success, errorMsg);
            };
        }

        std::string ep = entryPoint;
        if (ep.empty()) {
            ep = "main";
        }

        return mRenderer->UpdateShader(passName, shaderSource, language, cStage, ep, isShadertoy, cppCallback);
    }

    std::string GetWrappedGLSL(const std::string& glslSource) {
        return std::string(GLSL_SHADERTOY_HEADER) + glslSource;
    }

    void LoadMeshData(const emscripten::val& verticesArray, const emscripten::val& indicesArray) {
        // Stub for Phase 5.
    }

    void LoadTextureData(const std::string& name, const emscripten::val& pixelsArray, int width, int height, const std::string& format) {
        if (!mRenderer) return;

        // Defensive check: ensure pixelsArray is a valid object containing byteLength
        if (pixelsArray.isUndefined() || pixelsArray.isNull() || pixelsArray["byteLength"].isUndefined()) {
            std::cerr << "EngineCore: Invalid pixelsArray passed to LoadTextureData!" << std::endl;
            return;
        }

        unsigned int length = pixelsArray["byteLength"].as<unsigned int>();
        std::vector<uint8_t> buffer(length);
        
        // Copy JS typed array directly into the C++ vector using Emscripten EM_ASM
        MAIN_THREAD_EM_ASM({
            const pixelsArray = Emval.toValue($0);
            const destPtr = $1;
            const length = $2;
            const destView = new Uint8Array(HEAPU8.buffer, destPtr, length);
            destView.set(new Uint8Array(pixelsArray.buffer, pixelsArray.byteOffset, pixelsArray.byteLength));
        }, pixelsArray.as_handle(), buffer.data(), length);

        mRenderer->LoadTextureData(name, buffer.data(), static_cast<uint32_t>(width), static_cast<uint32_t>(height), format);
    }

    void SetPassInput(const std::string& passName, int channel, const std::string& inputName) {
        if (mRenderer) {
            mRenderer->SetPassInput(passName, channel, inputName);
        }
    }

    void SetPassEnabled(const std::string& passName, bool enabled) {
        if (mRenderer) {
            mRenderer->SetPassEnabled(passName, enabled);
        }
    }

    void SetUniforms(float time,
                     float timeDelta,
                     float frameRate,
                     int frame,
                     float mouseX,
                     float mouseY,
                     float mouseZ,
                     float mouseW) {
        if (mRenderer) {
            mRenderer->SetUniforms(time, timeDelta, frameRate, frame, mouseX, mouseY, mouseZ,
                                    mouseW);
        }
    }

    void Tick(double timestamp) {
        if (mRenderer) {
            mRenderer->Render();
        }
    }

    void Resize(int width, int height) {
        if (mRenderer) {
            mRenderer->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    }

    void SetGeometryType(int type) {
        if (mRenderer) {
            mRenderer->SetGeometryType(static_cast<GeometryType>(type));
        }
    }

  private:
    std::string mCanvasSelector;
    std::unique_ptr<WebContext> mContext;
    std::unique_ptr<Renderer> mRenderer;
};

EMSCRIPTEN_BINDINGS(engine_bindings) {
    emscripten::class_<EngineCore>("EngineCore")
        .constructor<std::string>()
        .function("initialize", &EngineCore::Initialize)
        .function("update_shader", &EngineCore::UpdateShader)
        .function("get_wrapped_glsl", &EngineCore::GetWrappedGLSL)
        .function("load_mesh_data", &EngineCore::LoadMeshData)
        .function("load_texture_data", &EngineCore::LoadTextureData)
        .function("set_pass_input", &EngineCore::SetPassInput)
        .function("set_pass_enabled", &EngineCore::SetPassEnabled)
        .function("set_uniforms", &EngineCore::SetUniforms)
        .function("tick", &EngineCore::Tick)
        .function("resize", &EngineCore::Resize)
        .function("set_geometry_type", &EngineCore::SetGeometryType);
}

