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

    template <typename Index>
    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<Index> Indices;
    };

    struct BoxGeometryProps
    {
        float Width{ 0.0f };
        float Height{ 0.0f };
        float Depth{ 0.0f };
        uint32_t SubdivisionCount{ 0 };
    };

    struct GridGeometryProps
    {
        float Width{ 0.0f };
        float Depth{ 0.0f };
        uint32_t RowCount{ 0 };
        uint32_t ColumnCount{ 0 };
    };

    struct CylinderGeometryProps
    {
        float TopRadius{ 0.0f };
        float BottomRadius{ 0.0f };
        float Height{ 0.0f };
        uint32_t SliceCount{ 0 };
        uint32_t StackCount{ 0 };
    };

    struct SphereGeometryProps
    {
        float Radius{ 0.0f };
        uint32_t SliceCount{ 0 };
        uint32_t StackCount{ 0 };
    };

    struct GeosphereGeometryProps
    {
        float Radius{ 0.0f };
        uint32_t SubdivisionCount{ 6 };
    };

    class GeometryGenerator
    {
    public:
        template <typename Index = uint32_t>
        static MeshData<Index> GenerateBox(const BoxGeometryProps& props);

        template <typename Index = uint32_t>
        static MeshData<Index> GenerateGrid(const GridGeometryProps& props);

        template <typename Index = uint32_t>
        static MeshData<Index> GenerateCylinder(const CylinderGeometryProps& props);

        template <typename Index = uint32_t>
        static MeshData<Index> GenerateSphere(const SphereGeometryProps& props);

        template <typename Index = uint32_t>
        static MeshData<Index> GenerateGeosphere(const GeosphereGeometryProps& props);

    private:
        template <typename Index>
        static void Subdivide(MeshData<Index>& meshData);

        static Vertex MiddlePoint(const Vertex& lhs, const Vertex& rhs);

        template <typename Index>
        static void GenerateCylinderTopCap(const CylinderGeometryProps& props, MeshData<Index>& meshData);

        template <typename Index>
        static void GenerateCylinderBottomCap(const CylinderGeometryProps& props, MeshData<Index>& meshData);
    };

} // namespace spieler::renderer

#include "geometry_generator.inl"
