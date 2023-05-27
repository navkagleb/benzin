#pragma once

namespace benzin
{

    struct MeshPrimitiveData;

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

        MeshPrimitiveData GenerateBox(const BoxConfig& config);
        MeshPrimitiveData GenerateGrid(const GridConfig& config);
        MeshPrimitiveData GenerateCylinder(const CylinderConfig& config);
        MeshPrimitiveData GenerateSphere(const SphereConfig& config);
        MeshPrimitiveData GenerateGeosphere(const GeosphereConfig& config);

    } // namespace geometry_generator

} // namespace benzin
