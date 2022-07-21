#include "spieler/config/bootstrap.hpp"

#if defined(SPIELER_GRAPHICS_API_DX12)

#include "platform/dx12/dx12_common.hpp"

#include "spieler/core/logger.hpp"

#include "spieler/renderer/blend_state.hpp"

namespace spieler::renderer::dx12
{

    D3D12_SHADER_VISIBILITY Convert(ShaderVisibility shaderVisibility)
    {
        switch (shaderVisibility)
        {
            case ShaderVisibility::All: return D3D12_SHADER_VISIBILITY_ALL;
            case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
            case ShaderVisibility::Hull: return D3D12_SHADER_VISIBILITY_HULL;
            case ShaderVisibility::Domain: return D3D12_SHADER_VISIBILITY_DOMAIN;
            case ShaderVisibility::Geometry: return D3D12_SHADER_VISIBILITY_GEOMETRY;
            case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;

            default:
            {
                SPIELER_WARNING("Unknown ShaderVisibility!");
                break;
            }
        }

        return D3D12_SHADER_VISIBILITY_ALL;
    }

    D3D12_PRIMITIVE_TOPOLOGY_TYPE Convert(PrimitiveTopologyType primitiveTopologyType)
    {
        switch (primitiveTopologyType)
        {
            case PrimitiveTopologyType::Unknown: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;

            case PrimitiveTopologyType::Point: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            case PrimitiveTopologyType::Line: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
            case PrimitiveTopologyType::Triangle: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            case PrimitiveTopologyType::Patch: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;

            default:
            {
                SPIELER_WARNING("Unknown PrimitiveTopologyType!");
                break;
            }
        }

        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }

    D3D12_PRIMITIVE_TOPOLOGY Convert(PrimitiveTopology primitiveTopology)
    {
        switch (primitiveTopology)
        {
            case PrimitiveTopology::Unknown: return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

            case PrimitiveTopology::PointList: return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            case PrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
            case PrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
            case PrimitiveTopology::TriangleList: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            case PrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;

            case PrimitiveTopology::ControlPointPatchlist1: return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist2: return D3D_PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist3: return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist4: return D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist5: return D3D_PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist6: return D3D_PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist7: return D3D_PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist8: return D3D_PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist9: return D3D_PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist10: return D3D_PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist11: return D3D_PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist12: return D3D_PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist13: return D3D_PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist14: return D3D_PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist15: return D3D_PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist16: return D3D_PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist17: return D3D_PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist18: return D3D_PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist19: return D3D_PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist20: return D3D_PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist21: return D3D_PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist22: return D3D_PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist23: return D3D_PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist24: return D3D_PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist25: return D3D_PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist26: return D3D_PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist27: return D3D_PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist28: return D3D_PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist29: return D3D_PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist30: return D3D_PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist31: return D3D_PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST;
            case PrimitiveTopology::ControlPointPatchlist32: return D3D_PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST;

            default:
            {
                SPIELER_WARNING("Unknown PrimitiveTopology!");
                break;
            }
        }

        return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
    }

    DXGI_FORMAT Convert(GraphicsFormat graphicsFormat)
    {
        switch (graphicsFormat)
        {
            case GraphicsFormat::Unknown: return DXGI_FORMAT_UNKNOWN;

            case GraphicsFormat::R32G32B32A32Typeless: return DXGI_FORMAT_R32G32B32A32_TYPELESS;
            case GraphicsFormat::R32G32B32A32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
            case GraphicsFormat::R32G32B32A32UnsignedInt: return DXGI_FORMAT_R32G32B32A32_UINT;
            case GraphicsFormat::R32G32B32A32SignedInt: return DXGI_FORMAT_R32G32B32A32_SINT;

            case GraphicsFormat::R32G32B32Typeless: return DXGI_FORMAT_R32G32B32_TYPELESS;
            case GraphicsFormat::R32G32B32Float: return DXGI_FORMAT_R32G32B32_FLOAT;
            case GraphicsFormat::R32G32B32UnsignedInt: return DXGI_FORMAT_R32G32B32_UINT;
            case GraphicsFormat::R32G32B32SignedInt: return DXGI_FORMAT_R32G32B32_SINT;

            case GraphicsFormat::R16G16B16A16Typeless: return DXGI_FORMAT_R16G16B16A16_TYPELESS;
            case GraphicsFormat::R16G16B16A16Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
            case GraphicsFormat::R16G16B16A16UnsignedNorm: return DXGI_FORMAT_R16G16B16A16_UNORM;
            case GraphicsFormat::R16G16B16A16UnsignedInt: return DXGI_FORMAT_R16G16B16A16_UINT;
            case GraphicsFormat::R16G16B16A16SignedNorm: return DXGI_FORMAT_R16G16B16A16_SNORM;
            case GraphicsFormat::R16G16B16A16SignedInt: return DXGI_FORMAT_R16G16B16A16_SINT;

            case GraphicsFormat::R32G32Typeless: return DXGI_FORMAT_R32G32_TYPELESS;
            case GraphicsFormat::R32G32Float: return DXGI_FORMAT_R32G32_FLOAT;
            case GraphicsFormat::R32G32UnsignedInt: return DXGI_FORMAT_R32G32_UINT;
            case GraphicsFormat::R32G32SignedInt: return DXGI_FORMAT_R32G32_SINT;

            case GraphicsFormat::R8G8B8A8Typeless: return DXGI_FORMAT_R8G8B8A8_TYPELESS;
            case GraphicsFormat::R8G8B8A8UnsignedNorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
            case GraphicsFormat::R8G8B8A8UnsignedInt: return DXGI_FORMAT_R8G8B8A8_UINT;
            case GraphicsFormat::R8G8B8A8SignedNorm: return DXGI_FORMAT_R8G8B8A8_SNORM;
            case GraphicsFormat::R8G8B8A8SignedInt: return DXGI_FORMAT_R8G8B8A8_SINT;

            case GraphicsFormat::R16G16Typeless: return DXGI_FORMAT_R16G16_TYPELESS;
            case GraphicsFormat::R16G16Float: return DXGI_FORMAT_R16G16_FLOAT;
            case GraphicsFormat::R16G16UnsignedNorm: return DXGI_FORMAT_R16G16_UNORM;
            case GraphicsFormat::R16G16UnsignedInt: return DXGI_FORMAT_R16G16_UINT;
            case GraphicsFormat::R16G16SignedNorm: return DXGI_FORMAT_R16G16_SNORM;
            case GraphicsFormat::R16G16SignedInt: return DXGI_FORMAT_R16G16_SINT;

            case GraphicsFormat::R32Typeless: return DXGI_FORMAT_R32_TYPELESS;
            case GraphicsFormat::D32Float: return DXGI_FORMAT_D32_FLOAT;
            case GraphicsFormat::R32Float: return DXGI_FORMAT_R32_FLOAT;
            case GraphicsFormat::R32UnsignedInt: return DXGI_FORMAT_R32_UINT;
            case GraphicsFormat::R32SignedInt: return DXGI_FORMAT_R32_SINT;

            case GraphicsFormat::D24UnsignedNormS8UnsignedInt: return DXGI_FORMAT_D24_UNORM_S8_UINT;

            case GraphicsFormat::R16Typeless: return DXGI_FORMAT_R16_TYPELESS;
            case GraphicsFormat::R16Float: return DXGI_FORMAT_R16_FLOAT;
            case GraphicsFormat::D16UnsignedNorm: return DXGI_FORMAT_D16_UNORM;
            case GraphicsFormat::R16UnsignedNorm: return DXGI_FORMAT_R16_UNORM;
            case GraphicsFormat::R16UnsignedInt: return DXGI_FORMAT_R16_UINT;
            case GraphicsFormat::R16SignedNorm: return DXGI_FORMAT_R16_SNORM;
            case GraphicsFormat::R16SignedInt: return DXGI_FORMAT_R16_SINT;

            default:
            {
                SPIELER_WARNING("Unknown GraphicsFormat!");
                break;
            }
        }

        return DXGI_FORMAT_UNKNOWN;
    }

    D3D12_BLEND_OP Convert(BlendState::Operation blendOperation)
    {
        switch (blendOperation)
        {
            case BlendState::Operation::Add: return D3D12_BLEND_OP_ADD;
            case BlendState::Operation::Substract: return D3D12_BLEND_OP_SUBTRACT;
            case BlendState::Operation::Min: return D3D12_BLEND_OP_MIN;
            case BlendState::Operation::Max: return D3D12_BLEND_OP_MAX;

            default:
            {
                SPIELER_WARNING("Unknown BlendState::Operation!");
                break;
            }
        }

        return D3D12_BLEND_OP_ADD;
    }

    D3D12_BLEND Convert(BlendState::ColorFactor blendColorFactor)
    {
        switch (blendColorFactor)
        {
        case BlendState::ColorFactor::Zero: return D3D12_BLEND_ZERO;
        case BlendState::ColorFactor::One: return D3D12_BLEND_ONE;
        case BlendState::ColorFactor::SourceColor: return D3D12_BLEND_SRC_COLOR;
        case BlendState::ColorFactor::InverseSourceColor: return D3D12_BLEND_INV_SRC_COLOR;
        case BlendState::ColorFactor::SourceAlpha: return D3D12_BLEND_SRC_ALPHA;
        case BlendState::ColorFactor::InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendState::ColorFactor::DesctinationAlpha: return D3D12_BLEND_DEST_ALPHA;
        case BlendState::ColorFactor::InverseDestinationAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendState::ColorFactor::DestinationColor: return D3D12_BLEND_DEST_COLOR;
        case BlendState::ColorFactor::InverseDestinationColor: return D3D12_BLEND_INV_DEST_COLOR;
        case BlendState::ColorFactor::SourceAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendState::ColorFactor::BlendFactor: return D3D12_BLEND_BLEND_FACTOR;
        case BlendState::ColorFactor::InverseBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;

            default:
            {
                SPIELER_WARNING("Unknown BlendState::ColorFactor!");
                break;
            }
        }

        return D3D12_BLEND_ZERO;
    }

    D3D12_BLEND Convert(BlendState::AlphaFactor blendAlphaFactor)
    {
        switch (blendAlphaFactor)
        {
            case BlendState::AlphaFactor::Zero: return D3D12_BLEND_ZERO;
            case BlendState::AlphaFactor::One: return D3D12_BLEND_ONE;
            case BlendState::AlphaFactor::SourceAlpha: D3D12_BLEND_SRC_ALPHA;
            case BlendState::AlphaFactor::InverseSourceAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
            case BlendState::AlphaFactor::DesctinationAlpha: return D3D12_BLEND_DEST_ALPHA;
            case BlendState::AlphaFactor::InverseDestinationAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
            case BlendState::AlphaFactor::SourceAlphaSaturated: return D3D12_BLEND_SRC_ALPHA_SAT;
            case BlendState::AlphaFactor::BlendFactor: return D3D12_BLEND_BLEND_FACTOR;
            case BlendState::AlphaFactor::InverseBlendFactor: return D3D12_BLEND_INV_BLEND_FACTOR;

            default:
            {
                SPIELER_WARNING("Unknown BlendState::AlphaFactor!");
                break;
            }
        };

        return D3D12_BLEND_ZERO;
    }

} // namespace spieler::renderer::dx12

#endif // defined(SPIELER_GRAPHICS_API_DX12)
