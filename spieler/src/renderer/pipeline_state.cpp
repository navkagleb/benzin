#include "renderer/pipeline_state.hpp"

namespace Spieler
{

    bool PipelineState::Init(const PipelineStateProps& props)
    {
        D3D12_SHADER_BYTECODE vertexShaderDesc{};
        vertexShaderDesc.pShaderBytecode    = props.VertexShader->GetData();
        vertexShaderDesc.BytecodeLength     = props.VertexShader->GetSize();

        D3D12_SHADER_BYTECODE pixelShaderDesc{};
        pixelShaderDesc.pShaderBytecode    = props.PixelShader->GetData();
        pixelShaderDesc.BytecodeLength     = props.PixelShader->GetSize();

        D3D12_STREAM_OUTPUT_DESC streamOutputDesc{};
        streamOutputDesc.pSODeclaration     = nullptr;
        streamOutputDesc.NumEntries         = 0;
        streamOutputDesc.pBufferStrides     = nullptr;
        streamOutputDesc.NumStrides         = 0;
        streamOutputDesc.RasterizedStream   = 0;

        D3D12_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable                 = false;
        blendDesc.IndependentBlendEnable                = false;
        blendDesc.RenderTarget[0].BlendEnable           = false;
        blendDesc.RenderTarget[0].LogicOpEnable         = false;
        blendDesc.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        D3D12_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode               = static_cast<D3D12_FILL_MODE>(props.RasterizerState->FillMode);
        rasterizerDesc.CullMode               = static_cast<D3D12_CULL_MODE>(props.RasterizerState->CullMode);
        rasterizerDesc.FrontCounterClockwise  = false;
        rasterizerDesc.DepthBias              = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp         = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias   = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable        = true;
        rasterizerDesc.MultisampleEnable      = false;
        rasterizerDesc.AntialiasedLineEnable  = false;
        rasterizerDesc.ForcedSampleCount      = 0;
        rasterizerDesc.ConservativeRaster     = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable        = true;
        depthStencilDesc.DepthWriteMask     = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc          = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable      = false;
        depthStencilDesc.StencilReadMask    = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilDesc.StencilWriteMask   = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depthStencilDesc.FrontFace          = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.BackFace           = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

        std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayout;
        d3d12InputLayout.reserve(props.InputLayout->size());

        std::uint32_t offset = 0;

        for (const InputLayoutElement& element : *props.InputLayout)
        {
            d3d12InputLayout.push_back(D3D12_INPUT_ELEMENT_DESC{
                element.Name.c_str(),
                0,
                element.Format,
                0,
                offset,
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            });

            offset += element.Size;
        }

        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
        inputLayoutDesc.pInputElementDescs  = d3d12InputLayout.data();
        inputLayoutDesc.NumElements         = static_cast<UINT>(d3d12InputLayout.size());

        D3D12_CACHED_PIPELINE_STATE cachedPipelineState{};
        cachedPipelineState.pCachedBlob             = nullptr;
        cachedPipelineState.CachedBlobSizeInBytes   = 0;

#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#else
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineStateDesc{};
        pipelineStateDesc.pRootSignature         = static_cast<ID3D12RootSignature*>(*props.RootSignature);
        pipelineStateDesc.VS                     = vertexShaderDesc;
        pipelineStateDesc.PS                     = pixelShaderDesc;
        pipelineStateDesc.StreamOutput           = streamOutputDesc;
        pipelineStateDesc.BlendState             = blendDesc;
        pipelineStateDesc.SampleMask             = 0xffffffff;
        pipelineStateDesc.RasterizerState        = rasterizerDesc;
        pipelineStateDesc.DepthStencilState      = depthStencilDesc;
        pipelineStateDesc.InputLayout            = inputLayoutDesc;
        pipelineStateDesc.IBStripCutValue        = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        pipelineStateDesc.PrimitiveTopologyType  = static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(props.PrimitiveTopologyType);
        pipelineStateDesc.NumRenderTargets       = 1;
        pipelineStateDesc.RTVFormats[0]          = props.RTVFormat;
        pipelineStateDesc.DSVFormat              = props.DSVFormat;
        pipelineStateDesc.SampleDesc.Count       = 1;
        pipelineStateDesc.SampleDesc.Quality     = 0;
        pipelineStateDesc.NodeMask               = 0;
        pipelineStateDesc.CachedPSO              = cachedPipelineState;
        pipelineStateDesc.Flags                  = flags;

        SPIELER_RETURN_IF_FAILED(GetDevice()->CreateGraphicsPipelineState(&pipelineStateDesc, __uuidof(ID3D12PipelineState), &m_Handle));

        return true;
    }

} // namespace Spieler