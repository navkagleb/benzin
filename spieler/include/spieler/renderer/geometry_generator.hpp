#pragma once

namespace spieler::renderer
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

    struct BoxGeometryConfig
    {
        float Width{ 0.0f };
        float Height{ 0.0f };
        float Depth{ 0.0f };
        uint32_t SubdivisionCount{ 0 };
    };

    struct GridGeometryConfig
    {
        float Width{ 0.0f };
        float Depth{ 0.0f };
        uint32_t RowCount{ 0 };
        uint32_t ColumnCount{ 0 };
    };

    struct CylinderGeometryConfig
    {
        float TopRadius{ 0.0f };
        float BottomRadius{ 0.0f };
        float Height{ 0.0f };
        uint32_t SliceCount{ 0 };
        uint32_t StackCount{ 0 };
    };

    struct SphereGeometryConfig
    {
        float Radius{ 0.0f };
        uint32_t SliceCount{ 0 };
        uint32_t StackCount{ 0 };
    };

    struct GeosphereGeometryConfig
    {
        float Radius{ 0.0f };
        uint32_t SubdivisionCount{ 6 };
    };

    class GeometryGenerator
    {
    public:
        static MeshData GenerateBox(const BoxGeometryConfig& props);
        static MeshData GenerateGrid(const GridGeometryConfig& props);
        static MeshData GenerateCylinder(const CylinderGeometryConfig& props);
        static MeshData GenerateSphere(const SphereGeometryConfig& props);
        static MeshData GenerateGeosphere(const GeosphereGeometryConfig& props);

    private:
        static Vertex MiddlePoint(const Vertex& lhs, const Vertex& rhs);

        static void Subdivide(MeshData& meshData);
        static void GenerateCylinderTopCap(const CylinderGeometryConfig& props, MeshData& meshData);
        static void GenerateCylinderBottomCap(const CylinderGeometryConfig& props, MeshData& meshData);
    };

} // namespace spieler::renderer
