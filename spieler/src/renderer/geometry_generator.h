#pragma once

#include <vector>

#include <DirectXMath.h>

namespace Spieler
{

    struct BasicVertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT3 Normal{};
        DirectX::XMFLOAT3 Tangent{};
        DirectX::XMFLOAT2 TexCoord{};
    };

    template <typename Vertex, typename Index>
    struct MeshData
    {
        std::vector<Vertex> Vertices;
        std::vector<Index>  Indices;
    };

    struct BoxGeometryProps
    {
        float           Width               = 0.0f;
        float           Height              = 0.0f;
        float           Depth               = 0.0f;
        std::uint32_t   SubdivisionCount    = 0;
    };

    struct GridGeometryProps
    {
        float           Width       = 0.0f;
        float           Depth       = 0.0f;
        std::uint32_t   RowCount    = 0;
        std::uint32_t   ColumnCount = 0;
    };

    struct CylinderGeometryProps
    {
        float           TopRadius       = 0.0f;
        float           BottomRadius    = 0.0f;
        float           Height          = 0.0f;
        std::uint32_t   SliceCount      = 0;
        std::uint32_t   StackCount      = 0;
    };

    struct SphereGeometryProps
    {
        float           Radius      = 0.0f;
        std::uint32_t   SliceCount  = 0;
        std::uint32_t   StackCount  = 0;
    };

    struct GeosphereGeometryProps
    {
        float           Radius              = 0.0f;
        std::uint32_t   SubdivisionCount    = 6;
    };

    class GeometryGenerator
    {
    public:
        template <typename Vertex = BasicVertex, typename Index = std::uint16_t>
        static MeshData<Vertex, Index> GenerateBox(const BoxGeometryProps& props);

        template <typename Vertex = BasicVertex, typename Index = std::uint16_t>
        static MeshData<Vertex, Index> GenerateGrid(const GridGeometryProps& props);

        template <typename Vertex = BasicVertex, typename Index = std::uint16_t>
        static MeshData<Vertex, Index> GenerateCylinder(const CylinderGeometryProps& props);

        template <typename Vertex = BasicVertex, typename Index = std::uint16_t>
        static MeshData<Vertex, Index> GenerateSphere(const SphereGeometryProps& props);

        template <typename Vertex = BasicVertex, typename Index = std::uint16_t>
        static MeshData<Vertex, Index> GenerateGeosphere(const GeosphereGeometryProps& props);

    private:
        template <typename Vertex, typename Index>
        static void Subdivide(MeshData<Vertex, Index>& meshData);

        template <typename Vertex>
        static Vertex MiddlePoint(const Vertex& lhs, const Vertex& rhs);

        template <typename Vertex, typename Index>
        static void GenerateCylinderTopCap(const CylinderGeometryProps& props, MeshData<Vertex, Index>& meshData);

        template <typename Vertex, typename Index>
        static void GenerateCylinderBottomCap(const CylinderGeometryProps& props, MeshData<Vertex, Index>& meshData);
    };

} // namespace Spieler

#include "geometry_generator.inl"