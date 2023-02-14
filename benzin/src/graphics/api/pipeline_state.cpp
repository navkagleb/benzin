#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/pipeline_state.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/root_signature.hpp"
#include "benzin/graphics/api/shader.hpp"
#include "benzin/graphics/api/render_states.hpp"

namespace benzin
{

    namespace
    {

        D3D12_SHADER_BYTECODE ConvertToD3D12Shader(const Shader* const shader)
        {
            return D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode{ shader ? shader->GetData() : nullptr },
                .BytecodeLength{ shader ? shader->GetSize() : 0 }
            };
        }

        D3D12_STREAM_OUTPUT_DESC GetDefaultD3D12StreatOutput()
        {
            return D3D12_STREAM_OUTPUT_DESC
            {
                .pSODeclaration{ nullptr },
                .NumEntries{ 0 },
                .pBufferStrides{ nullptr },
                .NumStrides{ 0 },
                .RasterizedStream{ 0 }
            };
        }

        D3D12_BLEND_DESC ConvertToD3D12BlendState(const BlendState* blendState)
        {
            if (!blendState)
            {
                blendState = &BlendState::GetDefault();
            }

            D3D12_BLEND_DESC d3d12BlendDesc
            {
                .AlphaToCoverageEnable{ static_cast<BOOL>(blendState->AlphaToCoverageState) },
                .IndependentBlendEnable{ static_cast<BOOL>(blendState->IndependentBlendState) }
            };

            for (size_t i = 0; i < blendState->RenderTargetStates.size(); ++i)
            {
                const BlendState::RenderTargetState& renderTargetState = blendState->RenderTargetStates[i];
                D3D12_RENDER_TARGET_BLEND_DESC& d3d12RenderTargetBlendDesc = d3d12BlendDesc.RenderTarget[i];

                switch (renderTargetState.State)
                {
                    case BlendState::RenderTargetState::State::Disabled:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = false;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;

                        break;
                    }
                    case BlendState::RenderTargetState::State::Enabled:
                    {
                        d3d12RenderTargetBlendDesc.BlendEnable = true;
                        d3d12RenderTargetBlendDesc.LogicOpEnable = false;
                        d3d12RenderTargetBlendDesc.SrcBlend = static_cast<D3D12_BLEND>(renderTargetState.ColorEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlend = static_cast<D3D12_BLEND>(renderTargetState.ColorEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOp = static_cast<D3D12_BLEND_OP>(renderTargetState.ColorEquation.Operation);
                        d3d12RenderTargetBlendDesc.SrcBlendAlpha = static_cast<D3D12_BLEND>(renderTargetState.AlphaEquation.SourceFactor);
                        d3d12RenderTargetBlendDesc.DestBlendAlpha = static_cast<D3D12_BLEND>(renderTargetState.AlphaEquation.DestinationFactor);
                        d3d12RenderTargetBlendDesc.BlendOpAlpha = static_cast<D3D12_BLEND_OP>(renderTargetState.AlphaEquation.Operation);

                        break;
                    }
                }

                d3d12RenderTargetBlendDesc.RenderTargetWriteMask = static_cast<UINT8>(renderTargetState.Channels);
            }

            return d3d12BlendDesc;
        }

        D3D12_RASTERIZER_DESC ConvertToD3D12RasterizerState(const RasterizerState* rasterizerState)
        {
            if (!rasterizerState)
            {
                rasterizerState = &RasterizerState::GetDefault();
            }

            return D3D12_RASTERIZER_DESC
            {
                .FillMode{ static_cast<D3D12_FILL_MODE>(rasterizerState->FillMode) },
                .CullMode{ static_cast<D3D12_CULL_MODE>(rasterizerState->CullMode) },
                .FrontCounterClockwise{ static_cast<BOOL>(rasterizerState->TriangleOrder) },
                .DepthBias{ rasterizerState->DepthBias },
                .DepthBiasClamp{ rasterizerState->DepthBiasClamp },
                .SlopeScaledDepthBias{ rasterizerState->SlopeScaledDepthBias },
                .DepthClipEnable{ true },
                .MultisampleEnable{ false },
                .AntialiasedLineEnable{ false },
                .ForcedSampleCount{ 0 },
                .ConservativeRaster{ D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF },
            };
        }

        D3D12_DEPTH_STENCIL_DESC ConvertToD3D12DepthStencilState(const DepthState* depthState, const StencilState* stencilState)
        {
            if (!depthState)
            {
                depthState = &DepthState::GetDefault();
            }

            if (!stencilState)
            {
                stencilState = &StencilState::GetDefault();
            }

            return D3D12_DEPTH_STENCIL_DESC
            {
                .DepthEnable{ static_cast<BOOL>(depthState->TestState) },
                .DepthWriteMask{ static_cast<D3D12_DEPTH_WRITE_MASK>(depthState->WriteState) },
                .DepthFunc{ static_cast<D3D12_COMPARISON_FUNC>(depthState->ComparisonFunction) },
                .StencilEnable{ static_cast<BOOL>(stencilState->TestState) },
                .StencilReadMask{ stencilState->ReadMask },
                .StencilWriteMask{ stencilState->WriteMask },
                .FrontFace
                {
                    .StencilFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState->FrontFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState->FrontFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ static_cast<D3D12_STENCIL_OP>(stencilState->FrontFaceBehaviour.PassOperation) },
                    .StencilFunc{ static_cast<D3D12_COMPARISON_FUNC>(stencilState->FrontFaceBehaviour.StencilFunction) },
                },
                .BackFace
                {
                    .StencilFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState->BackFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState->BackFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ static_cast<D3D12_STENCIL_OP>(stencilState->BackFaceBehaviour.PassOperation) },
                    .StencilFunc{ static_cast<D3D12_COMPARISON_FUNC>(stencilState->BackFaceBehaviour.StencilFunction) },
                }
            };
        }

        std::vector<D3D12_INPUT_ELEMENT_DESC> ConvertToD3D12InputLayoutElements(const std::vector<InputLayoutElement>* inputLayout)
        {
            if (!inputLayout || inputLayout->empty())
            {
                return {};
            }

            std::vector<D3D12_INPUT_ELEMENT_DESC> d3d12InputLayoutElements;
            d3d12InputLayoutElements.reserve(inputLayout->size());

            uint32_t offset = 0;

            for (const InputLayoutElement& element : *inputLayout)
            {
                d3d12InputLayoutElements.push_back(D3D12_INPUT_ELEMENT_DESC
                {
                    .SemanticName{ element.Name },
                    .SemanticIndex{ 0 },
                    .Format{ static_cast<DXGI_FORMAT>(element.Format) },
                    .InputSlot{ 0 },
                    .AlignedByteOffset{ offset },
                    .InputSlotClass{ D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA },
                    .InstanceDataStepRate{ 0 }
                });

                offset += element.Size;
            }

            return d3d12InputLayoutElements;
        }

        D3D12_INPUT_LAYOUT_DESC ConvertToD3D12InputLayout(const std::vector<D3D12_INPUT_ELEMENT_DESC>& inputLayout)
        {
            return D3D12_INPUT_LAYOUT_DESC
            {
                .pInputElementDescs{ inputLayout.data() },
                .NumElements{ static_cast<UINT>(inputLayout.size()) }
            };
        }

        D3D12_CACHED_PIPELINE_STATE GetDefauldD3D12CachedPipelineState()
        {
            return D3D12_CACHED_PIPELINE_STATE
            {
                .pCachedBlob{ nullptr },
                .CachedBlobSizeInBytes{ 0 }
            };
        }

    } // namespace _internal

    PipelineState::~PipelineState()
    {
        SafeReleaseD3D12Object(m_D3D12PipelineState);
    }

    GraphicsPipelineState::GraphicsPipelineState(Device& device, const Config& config, std::string_view debugName)
    {
        BENZIN_ASSERT(config.RootSignature.GetD3D12RootSignature());
        BENZIN_ASSERT(config.VertexShader.GetData());

        BENZIN_ASSERT(config.RenderTargetViewFormats.size() <= 8);

        const D3D12_SHADER_BYTECODE vertexShaderDesc = ConvertToD3D12Shader(&config.VertexShader);
        const D3D12_SHADER_BYTECODE pixelShaderDesc = ConvertToD3D12Shader(config.PixelShader);
        const D3D12_SHADER_BYTECODE domainShaderDesc = ConvertToD3D12Shader(config.DomainShader);
        const D3D12_SHADER_BYTECODE hullShaderDesc = ConvertToD3D12Shader(config.HullShader);
        const D3D12_SHADER_BYTECODE geometryShaderDesc = ConvertToD3D12Shader(config.GeometryShader);
        const D3D12_STREAM_OUTPUT_DESC streamOutputDesc = GetDefaultD3D12StreatOutput();
        const D3D12_BLEND_DESC blendDesc = ConvertToD3D12BlendState(config.BlendState);
        const D3D12_RASTERIZER_DESC rasterizerDesc = ConvertToD3D12RasterizerState(config.RasterizerState);
        const D3D12_DEPTH_STENCIL_DESC depthStencilDesc = ConvertToD3D12DepthStencilState(config.DepthState, config.StencilState);
        const std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutElements = ConvertToD3D12InputLayoutElements(config.InputLayout);
        const D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = ConvertToD3D12InputLayout(inputLayoutElements);
        const D3D12_CACHED_PIPELINE_STATE cachedPipelineState = GetDefauldD3D12CachedPipelineState();
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc
        {
            .pRootSignature{ config.RootSignature.GetD3D12RootSignature() },
            .VS{ vertexShaderDesc },
            .PS{ pixelShaderDesc },
            .DS{ domainShaderDesc },
            .HS{ hullShaderDesc },
            .GS{ geometryShaderDesc },
            .StreamOutput{ streamOutputDesc },
            .BlendState{ blendDesc },
            .SampleMask{ 0xffffffff },
            .RasterizerState{ rasterizerDesc },
            .DepthStencilState{ depthStencilDesc },
            .InputLayout{ inputLayoutDesc },
            .IBStripCutValue{ D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED },
            .PrimitiveTopologyType{ static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(config.PrimitiveTopologyType) },
            .NumRenderTargets{ static_cast<UINT>(config.RenderTargetViewFormats.size()) },
            .DSVFormat{ static_cast<DXGI_FORMAT>(config.DepthStencilViewFormat) },
            .SampleDesc{ 1, 0 },
            .NodeMask{ 0 },
            .CachedPSO{ cachedPipelineState },
            .Flags{ flags }
        };

        for (size_t i = 0; i < config.RenderTargetViewFormats.size(); ++i)
        {
            d3d12GraphicsPipelineStateDesc.RTVFormats[i] = static_cast<DXGI_FORMAT>(config.RenderTargetViewFormats[i]);
        }

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDebugName(debugName, true);
    }

    ComputePipelineState::ComputePipelineState(Device& device, const Config& config, std::string_view debugName)
    {
        BENZIN_ASSERT(config.RootSignature.GetD3D12RootSignature());
        BENZIN_ASSERT(config.ComputeShader.GetData());

        const D3D12_SHADER_BYTECODE computeShaderDesc = ConvertToD3D12Shader(&config.ComputeShader);
        const D3D12_CACHED_PIPELINE_STATE cachedPipelineState = GetDefauldD3D12CachedPipelineState();
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;

        const D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12ComputePipelineStateDesc
        {
            .pRootSignature{ config.RootSignature.GetD3D12RootSignature() },
            .CS{ computeShaderDesc },
            .NodeMask{ 0 },
            .CachedPSO{ cachedPipelineState },
            .Flags{ flags }
        };

        BENZIN_D3D12_ASSERT(device.GetD3D12Device()->CreateComputePipelineState(&d3d12ComputePipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDebugName(debugName, true);
    }

} // namespace benzin