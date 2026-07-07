#ifndef CORE_MESH_H_
#define CORE_MESH_H_

#include <cstdint>
#include <webgpu/webgpu_cpp.h>

#include <memory>
#include <vector>

struct Vertex {
    float position[3];
    float normal[3];
    float uv[2];
    float tangent[4];
};

class Mesh {
  public:
    Mesh(wgpu::Device device,
         const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    void Draw(wgpu::RenderPassEncoder renderPass) const;

    uint32_t GetIndexCount() const { return mIndexCount; }

    static std::unique_ptr<Mesh> CreateQuad(wgpu::Device device);
    static std::unique_ptr<Mesh> CreateCube(wgpu::Device device);
    static std::unique_ptr<Mesh> CreateSphere(wgpu::Device device, uint32_t subdivisions = 32);

  private:
    wgpu::Device mDevice = nullptr;
    wgpu::Buffer mVertexBuffer = nullptr;
    wgpu::Buffer mIndexBuffer = nullptr;
    uint32_t mVertexCount = 0;
    uint32_t mIndexCount = 0;
};

#endif  // CORE_MESH_H_
