#pragma once

namespace spieler
{

    enum class ShaderVisibility : uint8_t
    {
        All,
        Vertex,
        Hull,
        Domain,
        Geometry,
        Pixel,
    };

    enum class PrimitiveTopologyType : uint8_t
    {
        Unknown,

        Point,
        Line,
        Triangle,
        Patch
    };

    enum class PrimitiveTopology : uint8_t
    {
        Unknown,

        PointList,
        LineList,
        LineStrip,
        TriangleList,
        TriangleStrip,

        ControlPointPatchlist1,
        ControlPointPatchlist2,
        ControlPointPatchlist3,
        ControlPointPatchlist4,
        ControlPointPatchlist5,
        ControlPointPatchlist6,
        ControlPointPatchlist7,
        ControlPointPatchlist8,
        ControlPointPatchlist9,
        ControlPointPatchlist10,
        ControlPointPatchlist11,
        ControlPointPatchlist12,
        ControlPointPatchlist13,
        ControlPointPatchlist14,
        ControlPointPatchlist15,
        ControlPointPatchlist16,
        ControlPointPatchlist17,
        ControlPointPatchlist18,
        ControlPointPatchlist19,
        ControlPointPatchlist20,
        ControlPointPatchlist21,
        ControlPointPatchlist22,
        ControlPointPatchlist23,
        ControlPointPatchlist24,
        ControlPointPatchlist25,
        ControlPointPatchlist26,
        ControlPointPatchlist27,
        ControlPointPatchlist28,
        ControlPointPatchlist29,
        ControlPointPatchlist30,
        ControlPointPatchlist31,
        ControlPointPatchlist32,
    };

    enum class GraphicsFormat : uint8_t
    {
        Unknown,

        R32G32B32A32Typeless,
        R32G32B32A32Float,
        R32G32B32A32UnsignedInt,
        R32G32B32A32SignedInt,

        R32G32B32Typeless,
        R32G32B32Float,
        R32G32B32UnsignedInt,
        R32G32B32SignedInt,

        R16G16B16A16Typeless,
        R16G16B16A16Float,
        R16G16B16A16UnsignedNorm,
        R16G16B16A16UnsignedInt,
        R16G16B16A16SignedNorm,
        R16G16B16A16SignedInt,

        R32G32Typeless,
        R32G32Float,
        R32G32UnsignedInt,
        R32G32SignedInt,

        R8G8B8A8Typeless,
        R8G8B8A8UnsignedNorm,
        R8G8B8A8UnsignedInt,
        R8G8B8A8SignedNorm,
        R8G8B8A8SignedInt,

        R16G16Typeless,
        R16G16Float,
        R16G16UnsignedNorm,
        R16G16UnsignedInt,
        R16G16SignedNorm,
        R16G16SignedInt,

        R32Typeless,
        D32Float,
        R32Float,
        R32UnsignedInt,
        R32SignedInt,

        D24UnsignedNormS8UnsignedInt,

        R16Typeless,
        R16Float,
        D16UnsignedNorm,
        R16UnsignedNorm,
        R16UnsignedInt,
        R16SignedNorm,
        R16SignedInt,

        BlockCompression1UnsignedNormalized,
        BlockCompression3UnsignedNormalized
    };

    enum class ComparisonFunction : uint8_t
    {
        Never = D3D12_COMPARISON_FUNC_NEVER,
        Less = D3D12_COMPARISON_FUNC_LESS,
        Equal = D3D12_COMPARISON_FUNC_EQUAL,
        LessEqual = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        Greate = D3D12_COMPARISON_FUNC_GREATER,
        NotEqual = D3D12_COMPARISON_FUNC_NOT_EQUAL,
        GreateEqual = D3D12_COMPARISON_FUNC_GREATER_EQUAL,
        Always = D3D12_COMPARISON_FUNC_ALWAYS
    };

} // namespace spieler
