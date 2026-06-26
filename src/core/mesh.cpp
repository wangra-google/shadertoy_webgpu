#include "core/mesh.h"

#include <string.h>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Mesh::Mesh(WGPUDevice device,
           const std::vector<Vertex>& vertices,
           const std::vector<uint32_t>& indices)
    : m_device(device),
      m_vertexCount(static_cast<uint32_t>(vertices.size())),
      m_indexCount(static_cast<uint32_t>(indices.size())) {
    // Create vertex buffer
    WGPUBufferDescriptor vertexBufferDesc = {};
    vertexBufferDesc.label = {"Vertex Buffer", WGPU_STRLEN};
    vertexBufferDesc.size = vertices.size() * sizeof(Vertex);
    vertexBufferDesc.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst;
    vertexBufferDesc.mappedAtCreation = false;
    m_vertexBuffer = wgpuDeviceCreateBuffer(device, &vertexBufferDesc);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    wgpuQueueWriteBuffer(queue, m_vertexBuffer, 0, vertices.data(), vertexBufferDesc.size);

    // Create index buffer
    WGPUBufferDescriptor indexBufferDesc = {};
    indexBufferDesc.label = {"Index Buffer", WGPU_STRLEN};
    indexBufferDesc.size = indices.size() * sizeof(uint32_t);
    indexBufferDesc.usage = WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst;
    indexBufferDesc.mappedAtCreation = false;
    m_indexBuffer = wgpuDeviceCreateBuffer(device, &indexBufferDesc);

    wgpuQueueWriteBuffer(queue, m_indexBuffer, 0, indices.data(), indexBufferDesc.size);
    wgpuQueueRelease(queue);
}

Mesh::~Mesh() {
    if (m_vertexBuffer) {
        wgpuBufferDestroy(m_vertexBuffer);
        wgpuBufferRelease(m_vertexBuffer);
    }
    if (m_indexBuffer) {
        wgpuBufferDestroy(m_indexBuffer);
        wgpuBufferRelease(m_indexBuffer);
    }
}

void Mesh::Draw(WGPURenderPassEncoder renderPass) const {
    wgpuRenderPassEncoderSetVertexBuffer(renderPass, 0, m_vertexBuffer, 0,
                                         m_vertexCount * sizeof(Vertex));
    wgpuRenderPassEncoderSetIndexBuffer(renderPass, m_indexBuffer, WGPUIndexFormat_Uint32, 0,
                                        m_indexCount * sizeof(uint32_t));
    wgpuRenderPassEncoderDrawIndexed(renderPass, m_indexCount, 1, 0, 0, 0);
}

Mesh* Mesh::CreateQuad(WGPUDevice device) {
    std::vector<Vertex> vertices = {
        // Pos, Norm, UV, Tangent
        {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
    };

    std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

    return new Mesh(device, vertices, indices);
}

Mesh* Mesh::CreateCube(WGPUDevice device) {
    // 6 faces * 4 vertices = 24 vertices
    std::vector<Vertex> vertices = {
        // Front face (+Z)
        {{-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

        // Back face (-Z)
        {{0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f, 0.0f, 1.0f}},

        // Top face (+Y)
        {{-0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

        // Bottom face (-Y)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {0.0f, -1.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},

        // Right face (+X)
        {{0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
        {{0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
        {{0.5f, 0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},
        {{0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, -1.0f, 1.0f}},

        // Left face (-X)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-0.5f, -0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-0.5f, 0.5f, 0.5f}, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
        {{-0.5f, 0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}};

    std::vector<uint32_t> indices = {// Front
                                     0, 1, 2, 0, 2, 3,
                                     // Back
                                     4, 5, 6, 4, 6, 7,
                                     // Top
                                     8, 9, 10, 8, 10, 11,
                                     // Bottom
                                     12, 13, 14, 12, 14, 15,
                                     // Right
                                     16, 17, 18, 16, 18, 19,
                                     // Left
                                     20, 21, 22, 20, 22, 23};

    return new Mesh(device, vertices, indices);
}

Mesh* Mesh::CreateSphere(WGPUDevice device, uint32_t subdivisions) {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (uint32_t lat = 0; lat <= subdivisions; ++lat) {
        float theta = static_cast<float>(lat) * static_cast<float>(M_PI) / subdivisions;
        float sinTheta = sin(theta);
        float cosTheta = cos(theta);

        for (uint32_t lon = 0; lon <= subdivisions; ++lon) {
            float phi = static_cast<float>(lon) * 2.0f * static_cast<float>(M_PI) / subdivisions;
            float sinPhi = sin(phi);
            float cosPhi = cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            float u = static_cast<float>(lon) / subdivisions;
            float v = static_cast<float>(lat) / subdivisions;

            Vertex vertex;
            vertex.position[0] = x;
            vertex.position[1] = y;
            vertex.position[2] = z;

            vertex.normal[0] = x;
            vertex.normal[1] = y;
            vertex.normal[2] = z;

            vertex.uv[0] = u;
            vertex.uv[1] = v;

            vertex.tangent[0] = -sinPhi;
            vertex.tangent[1] = 0.0f;
            vertex.tangent[2] = cosPhi;
            vertex.tangent[3] = 1.0f;

            vertices.push_back(vertex);
        }
    }

    for (uint32_t lat = 0; lat < subdivisions; ++lat) {
        for (uint32_t lon = 0; lon < subdivisions; ++lon) {
            uint32_t first = (lat * (subdivisions + 1)) + lon;
            uint32_t second = first + subdivisions + 1;

            indices.push_back(first);
            indices.push_back(second);
            indices.push_back(first + 1);

            indices.push_back(second);
            indices.push_back(second + 1);
            indices.push_back(first + 1);
        }
    }

    return new Mesh(device, vertices, indices);
}
