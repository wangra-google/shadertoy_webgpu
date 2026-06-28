#ifndef CORE_MESH_H_
#define CORE_MESH_H_

#include <cstdint>
#include <webgpu/webgpu.h>

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
    Mesh(WGPUDevice device,
         const std::vector<Vertex>& vertices,
         const std::vector<uint32_t>& indices);
    ~Mesh();

    void Draw(WGPURenderPassEncoder renderPass) const;

    uint32_t GetIndexCount() const { return mIndexCount; }

    static std::unique_ptr<Mesh> CreateQuad(WGPUDevice device);
    static std::unique_ptr<Mesh> CreateCube(WGPUDevice device);
    static std::unique_ptr<Mesh> CreateSphere(WGPUDevice device, uint32_t subdivisions = 32);

  private:
    WGPUDevice mDevice = nullptr;
    WGPUBuffer mVertexBuffer = nullptr;
    WGPUBuffer mIndexBuffer = nullptr;
    uint32_t mVertexCount = 0;
    uint32_t mIndexCount = 0;
};

#endif  // CORE_MESH_H_
