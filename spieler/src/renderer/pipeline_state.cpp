#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/pipeline_state.hpp"

#include "spieler/renderer/root_signature.hpp"
#include "spieler/renderer/shader.hpp"
#include "spieler/renderer/blend_state.hpp"
#include "spieler/renderer/rasterizer_state.hpp"
#include "spieler/renderer/depth_stencil_state.hpp"
#include "spieler/renderer/renderer.hpp"

namespace spieler::renderer
{

    namespace _internal
    {

        static D3D12_SHADER_BYTECODE ConvertToD3D12Shader(const Shader* const shader)
        {
            return D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode = shader ? shader->GetData() : nullptr,
                .BytecodeLength = shader ? shader->GetSize() : 0
            };
        }

        static D3D12_STREAM_OUTPUT_DESC GetDefaultD3D12StreatOutput()
        {
            return D3D12_STREAM_OUTPUT_DESC
            {
                .pSODeclaration = nullptr,
                .NumEntries = 0,
                .pBufferStrides = nullptr,
                .NumStrides = 0,
                .RasterizedStream = 0
            };
        }

        static D3D12_BLEND_DESC GetDefaultD3D12BlendState()
        {
            const D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc
            {
                .BlendEnable = false,
                .LogicOpEnable = false,
                .SrcBlend = D3D12_BLEND_ONE,
                .DestBlend = D3D12_BLEND_ZERO,
                .BlendOp = D3D12_BLEND_OP_ADD,
                .SrcBlendAlpha = D3D12_BLEND_ONE,
                .DestBlendAlpha = D3D12_BLEND_ZERO,
                .BlendOpAlpha = D3D12_BLEND_OP_ADD,
                .LogicOp = D3D12_LOGIC_OP_NOOP,
                .RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL
            };

            return D3D12_BLEND_DESC
            {
                .AlphaToCoverageEnable = false,
                .IndependentBlendEnable = false,
                .RenderTarget = { renderTargetBlendDesc }
            };
        }

        static D3D12_BLEND_DESC ConvertToD3D12BlendState(const BlendState& blendState)
        {
            D3D12_BLEND_DESC d3d12BlendDesc
            {
                .AlphaToCoverageEnable = static_cast<bool>(blendState.AlphaToCoverageState),
                .IndependentBlendEnable = static_cast<bool>(blendState.IndependentBlendState)
            };

            for (std::size_t i = 0; i < blendState.RenderTargetProps.size(); ++i)
            {
                const RenderTargetBlendProps& renderTargetBlendProps{ blendState.RenderTargetProps[i] };
                D3D12_RENDER_TARGET_BLEND_DESC& d3d12RenderTargetBlendDesc{ d3d12BlendDesc.RenderTarget[i] };

                switch (renderTargetBlendProps.Type)
                {
                    case RenderTargetBlendingType::None:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = false;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;

                        break;
                    }
                    case RenderTargetBlendingType::Default:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = true;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;
                        d3d12RenderTargetBlendDesc.SrcBlend = static_cast<D3D12_BLEND>(renderTargetBlendProps.ColorEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlend = static_cast<D3D12_BLEND>(renderTargetBlendProps.ColorEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOp = static_cast<D3D12_BLEND_OP>(renderTargetBlendProps.ColorEquation.Operation);
                        d3d12RenderTargetBlendDesc.SrcBlendAlpha = static_cast<D3D12_BLEND>(renderTargetBlendProps.AlphaEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlendAlpha = static_cast<D3D12_BLEND>(renderTargetBlendProps.AlphaEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOpAlpha = static_cast<D3D12_BLEND_OP>(renderTargetBlendProps.AlphaEquation.Operation);

                        break;
                    }
                    case RenderTargetBlendingType::Logic:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = false;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = true;
                        d3d12RenderTargetBlendDesc.LogicOp = static_cast<D3D12_LOGIC_OP>(renderTargetBlendProps.LogicOperation);

                        break;
                    }
                }

                d3d12RenderTargetBlendDesc.RenderTargetWriteMask = renderTargetBlendProps.Channels;
            }

            return d3d12BlendDesc;
        }

        static D3D12_RASTERIZER_DESC ConvertToD3D12RasterizerState(const RasterizerState& rasterizerState)
        {
            return D3D12_RASTERIZER_DESC
            {
                .FillMode = static_cast<D3D12_FILL_MODE>(rasterizerState.FillMode),
                .CullMode = static_cast<D3D12_CULL_MODE>(rasterizerState.CullMode),
                .FrontCounterClockwise = static_cast<BOOL>(rasterizerState.TriangleOrder),
                .DepthBias = D3D12_DEFAULT_DEPTH_BIAS,
                .DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
                .SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
                .DepthClipEnable = true,
                .MultisampleEnable = false,
                .AntialiasedLineEnable = false,
                .ForcedSampleCount = 0,
                .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
            };
        }

        static D3D12_DEPTH_STENCIL_DESC ConvertToD3D12DepthStencilState(const DepthStencilState& depthStencilState)
        {
            return D3D12_DEPTH_STENCIL_DESC
            {
                .DepthEnable = static_cast<BOOL>(depthStencilState.DepthState.DepthTest),
                .DepthWriteMask = static_cast<D3D12_DEPTH_WRITE_MASK>(depthStencilState.DepthState.DepthWriteState),
                .DepthFunc = static_cast<D3D12_COMPARISON_FUNC>(depthStencilState.DepthState.ComparisonFunction),
                .StencilEnable = static_cast<BOOL>(depthStencilState.StencilState.StencilTest),
                .StencilReadMask = depthStencilState.StencilState.ReadMask,
                .StencilWriteMask = depthStencilState.StencilState.WriteMask,
                .FrontFace = D3D12_DEPTH_STENCILOP_DESC
                {
                    .StencilFailOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.FrontFace.StencilFailOperation),
                    .StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.FrontFace.DepthFailOperation),
                    .StencilPassOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.FrontFace.PassOperation),
                    .StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(depthStencilState.StencilState.FrontFace.StencilFunction),
                },
                .BackFace = D3D12_DEPTH_STENCILOP_DESC
                {
                    .StencilFailOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.BackFace.StencilFailOperation),
                    .StencilDepthFailOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.BackFace.DepthFailOperation),
                    .StencilPassOp = static_cast<D3D12_STENCIL_OP>(depthStencilState.StencilState.BackFace.PassOperation),
                    .StencilFunc = static_cast<D3D12_COMPARISON_FUNC>(depthStencilState.StencilState.BackFace.StencilFunction),
                },
            };
        }

        static std::vector<D3D12_INPUT_ELEMENT_DESC> ConvertToD3D12InputLayoutElements(const std::vector<InputLayoutElement>& inputLayout)
        {
            std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayoutElements;
            d3d12InputLayoutElements.reserve(inputLayout.size());

            uint32_t offset{ 0 };

            for (const InputLayoutElement& element : inputLayout)
            {
                d3d12InputLayoutElements.push_back(
                    D3D12_INPUT_ELEMENT_DESC
                    {
                        .SemanticName = element.Name.c_str(),
                        .SemanticIndex = 0,
                        .Format = static_cast<DXGI_FORMAT>(element.Format),
                        .InputSlot = 0,
                        .AlignedByteOffset = offset,
                        .InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                        .InstanceDataStepRate = 0
                    }
                );

                offset += element.Size;
            }

            return d3d12InputLayoutElements;
        }

        static D3D12_INPUT_LAYOUT_DESC ConvertToD3D12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
        {
            return D3D12_INPUT_LAYOUT_DESC
            {
                .pInputElementDescs = inputLayout.data(),
                .NumElements = static_cast<UINT>(inputLayout.size())
            };
        }

        static D3D12_CACHED_PIPELINE_STATE GetDefauldD3D12CachedPipelineState()
        {
            return D3D12_CACHED_PIPELINE_STATE
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0
            };
        }

    } // namespace _internal

    bool PipelineState::Init(const PipelineStateConfig& config)
    {
        SPIELER_ASSERT(config.RootSignature);
        SPIELER_ASSERT(config.VertexShader);
        SPIELER_ASSERT(config.PixelShader);
        SPIELER_ASSERT(config.RasterizerState);
        SPIELER_ASSERT(config.InputLayout);

        const D3D12_SHADER_BYTECODE vertexShaderDesc{ _internal::ConvertToD3D12Shader(config.VertexShader) };
        const D3D12_SHADER_BYTECODE pixelShaderDesc{ _internal::ConvertToD3D12Shader(config.PixelShader) };
        const D3D12_SHADER_BYTECODE geometryShaderDesc{ _internal::ConvertToD3D12Shader(config.GeometryShader) };
        const D3D12_STREAM_OUTPUT_DESC streamOutputDesc{ _internal::GetDefaultD3D12StreatOutput() };
        const D3D12_BLEND_DESC blendDesc{ config.BlendState ? _internal::ConvertToD3D12BlendState(*config.BlendState) : _internal::GetDefaultD3D12BlendState() };
        const D3D12_RASTERIZER_DESC rasterizerDesc{ _internal::ConvertToD3D12RasterizerState(*config.RasterizerState) };
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc{ _internal::ConvertToD3D12DepthStencilState(config.DepthStecilState ? *config.DepthStecilState : DepthStencilState{}) };
        const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutElements{ _internal::ConvertToD3D12InputLayoutElements(*config.InputLayout) };
        const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{ _internal::ConvertToD3D12InputLayout(inputLayoutElements) };
        const D3D12_CACHED_PIPELINE_STATE cachedPipelineState{ _internal::GetDefauldD3D12CachedPipelineState() };

#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#else
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

        const D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc
        {
            .pRootSignature = static_cast<ID3D12RootSignature*>(*config.RootSignature),
            .VS = vertexShaderDesc,
            .PS = pixelShaderDesc,
            .GS = geometryShaderDesc,
            .StreamOutput = streamOutputDesc,
            .BlendState = blendDesc,
            .SampleMask = 0xffffffff,
            .RasterizerState = rasterizerDesc,
            .DepthStencilState = depthStencilDesc,
            .InputLayout = inputLayoutDesc,
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(config.PrimitiveTopologyType),
            .NumRenderTargets = 1,
            .RTVFormats = { static_cast<DXGI_FORMAT>(config.RTVFormat) }, // RTVFormats in an array
            .DSVFormat = static_cast<DXGI_FORMAT>(config.DSVFormat),
            .SampleDesc = DXGI_SAMPLE_DESC{ 1, 0 },
            .NodeMask = 0,
            .CachedPSO = cachedPipelineState,
            .Flags = flags
        };

        SPIELER_RETURN_IF_FAILED(Renderer::GetInstance().GetDevice().GetNativeDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState), &m_PipelineState));

        return true;
    }

} // namespace spieler::renderer