#pragma once

#include "benzin/graphics/api/format.hpp"
#include "benzin/graphics/api/debug_name.hpp"

namespace benzin
{

    enum class ShaderVisibility : std::underlying_type_t<D3D12_SHADER_VISIBILITY>
    {
        All = D3D12_SHADER_VISIBILITY_ALL,
        Vertex = D3D12_SHADER_VISIBILITY_VERTEX,
        Hull = D3D12_SHADER_VISIBILITY_HULL,
        Domain = D3D12_SHADER_VISIBILITY_DOMAIN,
        Geometry = D3D12_SHADER_VISIBILITY_GEOMETRY,
        Pixel = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    struct ShaderRegister
    {
        uint32_t Index{ 0 };
        uint32_t Space{ 0 };
    };

    enum class PrimitiveTopologyType : std::underlying_type_t<D3D12_PRIMITIVE_TOPOLOGY_TYPE>
    {
        Unknown = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,

        Point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        Line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        Patch = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH,
    };

    enum class PrimitiveTopology : std::underlying_type_t<D3D12_PRIMITIVE_TOPOLOGY>
    {
        Unknown = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,

        PointList = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        LineList = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        LineStrip = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        TriangleList = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        TriangleStrip = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,

        ControlPointPatchlist4 = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
        ControlPointPatchlist16 = D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST,
    };

    enum class ComparisonFunction : std::underlying_type_t<D3D12_COMPARISON_FUNC>
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greater = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreaterEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS,
    };

    struct Viewport
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
        float MinDepth{ 0.0f };
        float MaxDepth{ 1.0f };
    };

    struct ScissorRect
    {
        float X{ 0.0f };
        float Y{ 0.0f };
        float Width{ 0.0f };
        float Height{ 0.0f };
    };

    constexpr uint32_t AlignThreadGroupCount(uint32_t value, uint32_t threadPerGroupCount)
    {
        return value / threadPerGroupCount + 1;
    }

} // namespace benzin
