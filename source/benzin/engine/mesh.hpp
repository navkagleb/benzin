#pragma once

#include "benzin/graphics/common.hpp"

namespace joint
{

    struct Material;
    struct MeshInfo;
    struct MeshInstance;
    struct MeshVertex;

}

namespace benzin
{

    class Buffer;

    struct MeshData
    {
        std::vector<joint::MeshVertex> Vertices;
        std::vector<uint32_t> Indices;

        PrimitiveTopology PrimitiveTopology = PrimitiveTopology::Unknown;

        std::optional<DirectX::BoundingBox> BoundingBox;
    };

    struct Mesh
    {
        std::vector<MeshData> SubMeshes;
        std::vector<joint::MeshInfo> SubMeshInfos;
        std::vector<joint::Material> Materials;
        std::vector<joint::MeshInstance> SubMeshInstances;

        uint32_t TotalVertexCount = 0;
        uint32_t TotalIndexCount = 0;
    };

    struct MeshGpuStorage
    {
        std::unique_ptr<Buffer> VertexBuffer;
        std::unique_ptr<Buffer> IndexBuffer;
        std::unique_ptr<Buffer> MeshInfoBuffer;
        std::unique_ptr<Buffer> MeshInstanceBuffer;
        std::unique_ptr<Buffer> MaterialBuffer;
    };

}
