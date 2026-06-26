#ifndef MESH_H
#define MESH_H

#include <stdint.h>
#include <webgpu/webgpu.h>

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

    uint32_t GetIndexCount() const { return m_indexCount; }

    static Mesh* CreateQuad(WGPUDevice device);
    static Mesh* CreateCube(WGPUDevice device);
    static Mesh* CreateSphere(WGPUDevice device, uint32_t subdivisions = 32);

  private:
    WGPUDevice m_device = nullptr;
    WGPUBuffer m_vertexBuffer = nullptr;
    WGPUBuffer m_indexBuffer = nullptr;
    uint32_t m_vertexCount = 0;
    uint32_t m_indexCount = 0;
};

#endif  // MESH_H
