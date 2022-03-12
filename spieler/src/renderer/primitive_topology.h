#pragma once

#include <cstdint>

#include <d3d12.h>

namespace Spieler
{

    enum PrimitiveTopologyType : std::uint32_t
    {
        PrimitiveTopologyType_Undefined = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
        PrimitiveTopologyType_Point     = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        PrimitiveTopologyType_Line      = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        PrimitiveTopologyType_Triangle  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        PrimitiveTopologyType_Patch     = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
    };

    enum PrimitiveTopology : std::uint32_t
    {
        PrimitiveTopology_Undefined         = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED,
        PrimitiveTopology_PointList         = D3D_PRIMITIVE_TOPOLOGY_POINTLIST,
        PrimitiveTopology_LineList          = D3D_PRIMITIVE_TOPOLOGY_LINELIST,
        PrimitiveTopology_LineStrip         = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,
        PrimitiveTopology_TriangleList      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,
        PrimitiveTopology_TriangleStrip     = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP,
        PrimitiveTopology_LineListAdj       = D3D_PRIMITIVE_TOPOLOGY_LINELIST_ADJ,
        PrimitiveTopology_LineStripAdj      = D3D_PRIMITIVE_TOPOLOGY_LINESTRIP_ADJ,
        PrimitiveTopology_TriangleListAdj   = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST_ADJ,
        PrimitiveTopology_TriangleStripAdj  = D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP_ADJ
    };

} // namespace Spieler