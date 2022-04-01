#include "renderer/pipeline_state.hpp"

#include "renderer/root_signature.hpp"
#include "renderer/shader.hpp"
#include "renderer/blend_state.hpp"
#include "renderer/rasterizer_state.hpp"

namespace Spieler
{

    namespace _Internal
    {

        static D3D12_SHADER_BYTECODE ToD3D12Shader(const Shader& shader)
        {
            D3D12_SHADER_BYTECODE d3d12Shader{};
            d3d12Shader.pShaderBytecode = shader.GetData();
            d3d12Shader.BytecodeLength = shader.GetSize();

            return d3d12Shader;
        }

        static D3D12_BLEND_DESC GetDefaultD3D12BlendState()
        {
            D3D12_BLEND_DESC d3d12BlendDesc{};
            d3d12BlendDesc.AlphaToCoverageEnable = false;
            d3d12BlendDesc.IndependentBlendEnable = false;
            d3d12BlendDesc.RenderTarget[0].BlendEnable = false;
            d3d12BlendDesc.RenderTarget[0].LogicOpEnable = false;
            d3d12BlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
            d3d12BlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
            d3d12BlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
            d3d12BlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
            d3d12BlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
            d3d12BlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
            d3d12BlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
            d3d12BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

            return d3d12BlendDesc;
        }

        static D3D12_BLEND_DESC ToD3D12BlendState(const BlendState& blendState)
        {
            D3D12_BLEND_DESC d3d12BlendDesc{};
            d3d12BlendDesc.AlphaToCoverageEnable = static_cast<bool>(blendState.AlphaToCoverageState);
            d3d12BlendDesc.IndependentBlendEnable = static_cast<bool>(blendState.IndependentBlendState);

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

        static std::vector<D3D12_INPUT_ELEMENT_DESC> ToD3D12InputLayoutElements(const std::vector<InputLayoutElement>& inputLayout)
        {
            std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayoutElements;
            d3d12InputLayoutElements.reserve(inputLayout.size());

            std::uint32_t offset{ 0 };

            for (const InputLayoutElement& element : inputLayout)
            {
                d3d12InputLayoutElements.push_back(
                    D3D12_INPUT_ELEMENT_DESC
                    {
                        element.Name.c_str(),
                        0,
                        element.Format,
                        0,
                        offset,
                        D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                        0
                    }
                );

                offset += element.Size;
            }

            return d3d12InputLayoutElements;
        }

        static D3D12_INPUT_LAYOUT_DESC ToD3D12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
        {
            D3D12_INPUT_LAYOUT_DESC d3d12InputLayoutDesc{};
            d3d12InputLayoutDesc.pInputElementDescs = inputLayout.data();
            d3d12InputLayoutDesc.NumElements = static_cast<UINT>(inputLayout.size());

            return d3d12InputLayoutDesc;
        }

    } // namespace _Internal

    bool PipelineState::Init(const PipelineStateProps& props)
    {
        SPIELER_ASSERT(props.RootSignature);
        SPIELER_ASSERT(props.VertexShader);
        SPIELER_ASSERT(props.PixelShader);
        SPIELER_ASSERT(props.InputLayout);

        D3D12_SHADER_BYTECODE vertexShaderDesc{ _Internal::ToD3D12Shader(*props.VertexShader) };
        D3D12_SHADER_BYTECODE pixelShaderDesc{ _Internal::ToD3D12Shader(*props.PixelShader) };

        D3D12_STREAM_OUTPUT_DESC streamOutputDesc{};
        streamOutputDesc.pSODeclaration = nullptr;
        streamOutputDesc.NumEntries = 0;
        streamOutputDesc.pBufferStrides = nullptr;
        streamOutputDesc.NumStrides = 0;
        streamOutputDesc.RasterizedStream = 0;

        D3D12_BLEND_DESC blendDesc{ props.BlendState ? _Internal::ToD3D12BlendState(*props.BlendState) : _Internal::GetDefaultD3D12BlendState() };

        D3D12_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode = static_cast<D3D12_FILL_MODE>(props.RasterizerState->FillMode);
        rasterizerDesc.CullMode = static_cast<D3D12_CULL_MODE>(props.RasterizerState->CullMode);
        rasterizerDesc.FrontCounterClockwise = false;
        rasterizerDesc.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable = true;
        rasterizerDesc.MultisampleEnable = false;
        rasterizerDesc.AntialiasedLineEnable = false;
        rasterizerDesc.ForcedSampleCount = 0;
        rasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable = true;
        depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable = false;
        depthStencilDesc.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilDesc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depthStencilDesc.FrontFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.BackFace = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

        std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutElements{ _Internal::ToD3D12InputLayoutElements(*props.InputLayout) };
        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{ _Internal::ToD3D12InputLayout(inputLayoutElements) };

        D3D12_CACHED_PIPELINE_STATE cachedPipelineState{};
        cachedPipelineState.pCachedBlob = nullptr;
        cachedPipelineState.CachedBlobSizeInBytes = 0;

#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#else
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
        pipelineStateDesc.pRootSignature = static_cast<ID3D12RootSignature*>(*props.RootSignature);
        pipelineStateDesc.VS = vertexShaderDesc;
        pipelineStateDesc.PS = pixelShaderDesc;
        pipelineStateDesc.StreamOutput = streamOutputDesc;
        pipelineStateDesc.BlendState = blendDesc;
        pipelineStateDesc.SampleMask = 0xffffffff;
        pipelineStateDesc.RasterizerState = rasterizerDesc;
        pipelineStateDesc.DepthStencilState = depthStencilDesc;
        pipelineStateDesc.InputLayout = inputLayoutDesc;
        pipelineStateDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        pipelineStateDesc.PrimitiveTopologyType = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(props.PrimitiveTopologyType);
        pipelineStateDesc.NumRenderTargets = 1;
        pipelineStateDesc.RTVFormats[0] = props.RTVFormat;
        pipelineStateDesc.DSVFormat = props.DSVFormat;
        pipelineStateDesc.SampleDesc.Count = 1;
        pipelineStateDesc.SampleDesc.Quality = 0;
        pipelineStateDesc.NodeMask = 0;
        pipelineStateDesc.CachedPSO = cachedPipelineState;
        pipelineStateDesc.Flags = flags;

        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState), &m_Handle));

        return true;
    }

} // namespace Spieler