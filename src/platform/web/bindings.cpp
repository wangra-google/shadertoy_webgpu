#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <iostream>
#include <memory>
#include <string>

#include "core/renderer.h"
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
                      const std::string& wgslSource,
                      const std::string& language) {
        if (!mRenderer) {
            return false;
        }
        // In Phase 1, passName and language are ignored, compiling directly to renderer.
        return mRenderer->UpdateShader(wgslSource);
    }

    void LoadMeshData(const emscripten::val& verticesArray, const emscripten::val& indicesArray) {
        // Stub for Phase 1. Will be implemented in Phase 4.
    }

    void LoadTextureData(int channel, const emscripten::val& pixelsArray, int width, int height) {
        // Stub for Phase 1. Will be implemented in Phase 4.
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
        .function("load_mesh_data", &EngineCore::LoadMeshData)
        .function("load_texture_data", &EngineCore::LoadTextureData)
        .function("set_uniforms", &EngineCore::SetUniforms)
        .function("tick", &EngineCore::Tick)
        .function("resize", &EngineCore::Resize)
        .function("set_geometry_type", &EngineCore::SetGeometryType);
}
