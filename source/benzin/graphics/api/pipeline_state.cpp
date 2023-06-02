#include "benzin/config/bootstrap.hpp"

#include "benzin/graphics/api/pipeline_state.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/render_states.hpp"
#include "benzin/graphics/api/shaders.hpp"

namespace benzin
{

    namespace
    {

        D3D12_SHADER_BYTECODE ConvertToD3D12Shader(const PipelineState::Shader& shader, ShaderType shaderType)
        {
            if (!shader.IsValid())
            {
                return {};
            }

            const std::vector<std::byte>& shaderByteCode = GetShaderBlob(shader.FileName, shader.EntryPoint, shaderType, shader.Defines);

            return D3D12_SHADER_BYTECODE
            {
                .pShaderBytecode{ shaderByteCode.data() },
                .BytecodeLength{ shaderByteCode.size() }
            };
        }

        D3D12_BLEND_DESC ConvertToD3D12BlendState(const BlendState& blendState)
        {
            D3D12_BLEND_DESC d3d12BlendDesc
            {
                .AlphaToCoverageEnable{ static_cast<BOOL>(blendState.AlphaToCoverageState) },
                .IndependentBlendEnable{ static_cast<BOOL>(blendState.IndependentBlendState) }
            };

            for (size_t i = 0; i < blendState.RenderTargetStates.size(); ++i)
            {
                const BlendState::RenderTargetState& renderTargetState = blendState.RenderTargetStates[i];
                D3D12_RENDER_TARGET_BLEND_DESC& d3d12RenderTargetBlendDesc = d3d12BlendDesc.RenderTarget[i];

                if (!renderTargetState.IsEnabled)
                {
                    d3d12RenderTargetBlendDesc.BlendEnable = false;
                    d3d12RenderTargetBlendDesc.LogicOpEnable = false;
                }
                else
                {
                    d3d12RenderTargetBlendDesc.BlendEnable = true;
                    d3d12RenderTargetBlendDesc.LogicOpEnable = false;
                    d3d12RenderTargetBlendDesc.SrcBlend = static_cast<D3D12_BLEND>(renderTargetState.ColorEquation.SourceFactor);
                    d3d12RenderTargetBlendDesc.DestBlend = static_cast<D3D12_BLEND>(renderTargetState.ColorEquation.DestinationFactor);
                    d3d12RenderTargetBlendDesc.BlendOp = static_cast<D3D12_BLEND_OP>(renderTargetState.ColorEquation.Operation);
                    d3d12RenderTargetBlendDesc.SrcBlendAlpha = static_cast<D3D12_BLEND>(renderTargetState.AlphaEquation.SourceFactor);
                    d3d12RenderTargetBlendDesc.DestBlendAlpha = static_cast<D3D12_BLEND>(renderTargetState.AlphaEquation.DestinationFactor);
                    d3d12RenderTargetBlendDesc.BlendOpAlpha = static_cast<D3D12_BLEND_OP>(renderTargetState.AlphaEquation.Operation);
                }

                d3d12RenderTargetBlendDesc.RenderTargetWriteMask = static_cast<UINT8>(renderTargetState.Channels);
            }

            return d3d12BlendDesc;
        }

        D3D12_RASTERIZER_DESC ConvertToD3D12RasterizerState(const RasterizerState& rasterizerState)
        {
            const BOOL isClockwise = rasterizerState.TriangleOrder == RasterizerState::TriangleOrder::Clockwise;

            return D3D12_RASTERIZER_DESC
            {
                .FillMode{ static_cast<D3D12_FILL_MODE>(rasterizerState.FillMode) },
                .CullMode{ static_cast<D3D12_CULL_MODE>(rasterizerState.CullMode) },
                .FrontCounterClockwise{ !isClockwise },
                .DepthBias{ rasterizerState.DepthBias },
                .DepthBiasClamp{ rasterizerState.DepthBiasClamp },
                .SlopeScaledDepthBias{ rasterizerState.SlopeScaledDepthBias },
                .DepthClipEnable{ true },
                .MultisampleEnable{ false },
                .AntialiasedLineEnable{ false },
                .ForcedSampleCount{ 0 },
                .ConservativeRaster{ D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF },
            };
        }

        D3D12_DEPTH_STENCIL_DESC ConvertToD3D12DepthStencilState(const DepthState& depthState, const StencilState& stencilState)
        {
            return D3D12_DEPTH_STENCIL_DESC
            {
                .DepthEnable{ static_cast<BOOL>(depthState.IsEnabled) },
                .DepthWriteMask{ static_cast<D3D12_DEPTH_WRITE_MASK>(depthState.IsWriteEnabled) },
                .DepthFunc{ static_cast<D3D12_COMPARISON_FUNC>(depthState.ComparisonFunction) },
                .StencilEnable{ static_cast<BOOL>(stencilState.IsEnabled) },
                .StencilReadMask{ stencilState.ReadMask },
                .StencilWriteMask{ stencilState.WriteMask },
                .FrontFace
                {
                    .StencilFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ static_cast<D3D12_STENCIL_OP>(stencilState.FrontFaceBehaviour.PassOperation) },
                    .StencilFunc{ static_cast<D3D12_COMPARISON_FUNC>(stencilState.FrontFaceBehaviour.StencilFunction) },
                },
                .BackFace
                {
                    .StencilFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.StencilFailOperation) },
                    .StencilDepthFailOp{ static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.DepthFailOperation) },
                    .StencilPassOp{ static_cast<D3D12_STENCIL_OP>(stencilState.BackFaceBehaviour.PassOperation) },
                    .StencilFunc{ static_cast<D3D12_COMPARISON_FUNC>(stencilState.BackFaceBehaviour.StencilFunction) },
                }
            };
        }

    } // anonymous namespace

    PipelineState::PipelineState(Device& device, const GraphicsConfig& config)
    {
        BENZIN_ASSERT(device.GetD3D12Device());
        BENZIN_ASSERT(device.GetD3D12BindlessRootSignature());

        BENZIN_ASSERT(config.VertexShader.IsValid());
        BENZIN_ASSERT(config.RenderTargetViewFormats.size() <= 8);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc
        {
            .pRootSignature{ device.GetD3D12BindlessRootSignature() },
            .VS{ ConvertToD3D12Shader(config.VertexShader, ShaderType::Vertex) },
            .PS{ ConvertToD3D12Shader(config.PixelShader, ShaderType::Pixel) },
            .DS{ nullptr, 0 },
            .HS{ nullptr, 0 },
            .GS{ nullptr, 0 },
            .StreamOutput
            {
                .pSODeclaration{ nullptr },
                .NumEntries{ 0 },
                .pBufferStrides{ nullptr },
                .NumStrides{ 0 },
                .RasterizedStream{ 0 },
            },
            .BlendState{ ConvertToD3D12BlendState(config.BlendState) },
            .SampleMask{ 0xffffffff },
            .RasterizerState{ ConvertToD3D12RasterizerState(config.RasterizerState) },
            .DepthStencilState{ ConvertToD3D12DepthStencilState(config.DepthState, config.StencilState) },
            .InputLayout
            {
                .pInputElementDescs{ nullptr },
                .NumElements{ 0 },
            },
            .IBStripCutValue{ D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED },
            .PrimitiveTopologyType{ static_cast<D3D12_PRIMITIVE_TOPOLOGY_TYPE>(config.PrimitiveTopologyType) },
            .NumRenderTargets{ static_cast<UINT>(config.RenderTargetViewFormats.size()) },
            .DSVFormat{ static_cast<DXGI_FORMAT>(config.DepthStencilViewFormat) },
            .SampleDesc{ 1, 0 },
            .NodeMask{ 0 },
            .CachedPSO
            {
                .pCachedBlob{ nullptr },
                .CachedBlobSizeInBytes{ 0 },
            },
            .Flags{ D3D12_PIPELINE_STATE_FLAG_NONE },
        };

        for (size_t i = 0; i < config.RenderTargetViewFormats.size(); ++i)
        {
            d3d12GraphicsPipelineStateDesc.RTVFormats[i] = static_cast<DXGI_FORMAT>(config.RenderTargetViewFormats[i]);
        }

        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
    }

    PipelineState::PipelineState(Device& device, const ComputeConfig& config)
    {
        BENZIN_ASSERT(device.GetD3D12Device());
        BENZIN_ASSERT(device.GetD3D12BindlessRootSignature());

        BENZIN_ASSERT(config.ComputeShader.IsValid());

        const D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12ComputePipelineStateDesc
        {
            .pRootSignature{ device.GetD3D12BindlessRootSignature() },
            .CS{ ConvertToD3D12Shader(config.ComputeShader, ShaderType::Compute) },
            .NodeMask{ 0 },
            .CachedPSO
            {
                .pCachedBlob{ nullptr },
                .CachedBlobSizeInBytes{ 0 },
            },
            .Flags{ D3D12_PIPELINE_STATE_FLAG_NONE },
        };

        BENZIN_HR_ASSERT(device.GetD3D12Device()->CreateComputePipelineState(&d3d12ComputePipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
    }

    PipelineState::~PipelineState()
    {
        dx::SafeRelease(m_D3D12PipelineState);
    }

} // namespace benzin
