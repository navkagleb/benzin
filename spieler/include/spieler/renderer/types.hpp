#pragma once

namespace spieler::renderer
{

    enum class ShaderVisibility : uint8_t
    {
        All = D3D12_SHADER_VISIBILITY_ALL,
        Vertex = D3D12_SHADER_VISIBILITY_VERTEX,
        Hull = D3D12_SHADER_VISIBILITY_HULL,
        Domain = D3D12_SHADER_VISIBILITY_DOMAIN,
        Geometry = D3D12_SHADER_VISIBILITY_GEOMETRY,
        Pixel = D3D12_SHADER_VISIBILITY_PIXEL,
    };

    enum class PrimitiveTopologyType : uint8_t
    {
        Undefined = D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED,
        Point = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT,
        Line = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
        Triangle = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
        Patch = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH
    };

    enum class PrimitiveTopology : uint8_t
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

    // Take from DXGI_FORMAT
    enum class GraphicsFormat : uint8_t
    {
        Unknown	= 0,

        R32G32B32A32Typeless = 1,
        R32G32B32A32Float = 2,
        R32G32B32A32UnsignedInt = 3,
        R32G32B32A32SignedInt = 4,

        R32G32B32Typeless = 5,
        R32G32B32Float = 6,
        R32G32B32UnsignedInt = 7,
        R32G32B32SignedInt = 8,

        R16G16B16A16Typeless = 9,
        R16G16B16A16Float = 10,
        R16G16B16A16UnsignedNorm = 11,
        R16G16B16A16UnsignedInt = 12,
        R16G16B16A16SignedNorm = 13,
        R16G16B16A16SignedInt = 14,

        R32G32Typeless = 15,
        R32G32Float = 16,
        R32G32UnsignedInt = 17,
        R32G32SignedInt = 18,

        R8G8B8A8Typeless = 27,
        R8G8B8A8UnsignedNorm = 28,
        R8G8B8A8UnsignedInt = 30,
        R8G8B8A8SignedNorm = 31,
        R8G8B8A8SignedInt = 32,

        R16G16Typeless = 33,
        R16G16Float = 34,
        R16G16UnsignedNorm = 35,
        R16G16UnsignedInt = 36,
        R16G16SignedNorm = 37,
        R16G16SignedInt = 38,

        R32Typeless= 39,
        D32Float = 40,
        R32Float = 41,
        R32UnsignedInt = 42,
        R32SignedInt = 43,

        R24G8Typelss = 44,
        D24UnsignedNormS8UnsignedInt= 45,

        R16Typeless = 53,
        R16Float = 54,
        D16UnsignedNorm = 55,
        R16UnsignedNorm = 56,
        R16UnsignedInt = 57,
        R16SignedNorm = 58,
        R16SignedInt = 59,
    };

} // namespace spieler::renderer