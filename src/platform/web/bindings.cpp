#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <iostream>
#include <string>

#include "core/renderer.h"
#include "platform/web/web_context.h"

class EngineCore {
  public:
    EngineCore(std::string canvas_selector)
        : m_canvasSelector(canvas_selector), m_context(nullptr), m_renderer(nullptr) {}

    ~EngineCore() {
        delete m_renderer;
        delete m_context;
    }

    bool Initialize() {
        m_context = new WebContext(m_canvasSelector);
        if (!m_context->Init()) {
            std::cerr << "EngineCore: WebContext Init failed!" << std::endl;
            return false;
        }

        m_renderer = new Renderer(m_context);
        if (!m_renderer->Init()) {
            std::cerr << "EngineCore: Renderer Init failed!" << std::endl;
            return false;
        }

        return true;
    }

    bool UpdateShader(const std::string& pass_name,
                      const std::string& wgsl_source,
                      const std::string& language) {
        if (!m_renderer) {
            return false;
        }
        // In Phase 1, pass_name and language are ignored, compiling directly to renderer.
        return m_renderer->UpdateShader(wgsl_source);
    }

    void LoadMeshData(const emscripten::val& vertices_array, const emscripten::val& indices_array) {
        // Stub for Phase 1. Will be implemented in Phase 4.
    }

    void LoadTextureData(int channel, const emscripten::val& pixels_array, int width, int height) {
        // Stub for Phase 1. Will be implemented in Phase 4.
    }

    void SetUniforms(float time,
                     float time_delta,
                     float frame_rate,
                     int frame,
                     float mouse_x,
                     float mouse_y,
                     float mouse_z,
                     float mouse_w) {
        if (m_renderer) {
            m_renderer->SetUniforms(time, time_delta, frame_rate, frame, mouse_x, mouse_y, mouse_z,
                                    mouse_w);
        }
    }

    void Tick(double timestamp) {
        if (m_renderer) {
            m_renderer->Render();
        }
    }

    void Resize(int width, int height) {
        if (m_renderer) {
            m_renderer->Resize(static_cast<uint32_t>(width), static_cast<uint32_t>(height));
        }
    }

    void SetGeometryType(int type) {
        if (m_renderer) {
            m_renderer->SetGeometryType(static_cast<GeometryType>(type));
        }
    }

  private:
    std::string m_canvasSelector;
    WebContext* m_context;
    Renderer* m_renderer;
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
