#pragma once

namespace benzin
{

    struct MeshData;

    struct BoxGeometryCreation
    {
        float Width = 0.0f;
        float Height = 0.0f;
        float Depth = 0.0f;
        uint32_t SubdivisionCount = 0;
    };

    struct GridGeometryCreation
    {
        float Width = 0.0f;
        float Depth = 0.0f;
        uint32_t RowCount = 0;
        uint32_t ColumnCount = 0;
    };

    struct CylinderGeometryCreation
    {
        float TopRadius = 0.0f;
        float BottomRadius = 0.0f;
        float Height = 0.0f;
        uint32_t SliceCount = 0;
        uint32_t StackCount = 0;
    };

    struct SphereGeometryCreation
    {
        float Radius = 0.0f;
        uint32_t SliceCount = 0;
        uint32_t StackCount = 0;
    };

    struct GeosphereGeometryCreation
    {
        float Radius = 0.0f;
        uint32_t SubdivisionCount = 6;
    };

    MeshData GenerateBox(const BoxGeometryCreation& creation);
    MeshData GenerateGrid(const GridGeometryCreation& creation);
    MeshData GenerateCylinder(const CylinderGeometryCreation& creation);
    MeshData GenerateSphere(const SphereGeometryCreation& creation);
    MeshData GenerateGeosphere(const GeosphereGeometryCreation& creation);

} // namespace benzin
