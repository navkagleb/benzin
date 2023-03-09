#pragma once

namespace benzin
{

    struct Vertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT3 Normal{};
        DirectX::XMFLOAT3 Tangent{};
        DirectX::XMFLOAT2 TexCoord{};
    };

    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
    };

    namespace geometry_generator
    {

        struct BoxConfig
        {
            float Width{ 0.0f };
            float Height{ 0.0f };
            float Depth{ 0.0f };
            uint32_t SubdivisionCount{ 0 };
        };

        struct GridConfig
        {
            float Width{ 0.0f };
            float Depth{ 0.0f };
            uint32_t RowCount{ 0 };
            uint32_t ColumnCount{ 0 };
        };

        struct CylinderConfig
        {
            float TopRadius{ 0.0f };
            float BottomRadius{ 0.0f };
            float Height{ 0.0f };
            uint32_t SliceCount{ 0 };
            uint32_t StackCount{ 0 };
        };

        struct SphereConfig
        {
            float Radius{ 0.0f };
            uint32_t SliceCount{ 0 };
            uint32_t StackCount{ 0 };
        };

        struct GeosphereConfig
        {
            float Radius{ 0.0f };
            uint32_t SubdivisionCount{ 6 };
        };

        MeshData GenerateBox(const BoxConfig& config);
        MeshData GenerateGrid(const GridConfig& config);
        MeshData GenerateCylinder(const CylinderConfig& config);
        MeshData GenerateSphere(const SphereConfig& config);
        MeshData GenerateGeosphere(const GeosphereConfig& config);

    } // namesapce geometry_generator

} // namespace benzin
