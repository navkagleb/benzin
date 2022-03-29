#pragma once

#include <cstdint>

#include <d3d12.h>

namespace Spieler
{

    enum class PrimitiveTopologyType : std::uint32_t
    {
        Undefined = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
        Point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        Line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        Patch = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
    };

    enum class PrimitiveTopology : std::uint32_t
    {
        Undefined = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
        PointList = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        LineList = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        LineStrip = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        TriangleList = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        TriangleStrip = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        LineListAdj = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
        LineStripAdj = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        TriangleListAdj = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
        TriangleStripAdj = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ
    };

} // namespace Spieler