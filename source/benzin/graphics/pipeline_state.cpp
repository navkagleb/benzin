#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pipeline_state.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/render_states.hpp"
#include "benzin/graphics/unified_root_signature.hpp"

namespace benzin
{

    static constexpr auto g_DefaultShaderEntryPoints = []
    {
        std::array<std::string_view, +ShaderType::ShaderCount> entryPoints;
        entryPoints[+ShaderType::Vertex] = "VsMain";
        entryPoints[+ShaderType::Pixel] = "PsMain";
        entryPoints[+ShaderType::Compute] = "CsMain";

        return entryPoints;
    }();

    static D3D12_SHADER_BYTECODE ToD3D12Shader(Device& device, const ShaderInfo& shader, bool isShaderCacheIgnored)
    {
        if (!shader.IsValid())
        {
            return { nullptr, 0 };
        }
        
        auto& shaderManager = device.GetBackend().GetShaderManager();
        const std::span shaderDxil = shaderManager.GetShaderDxil(shader, isShaderCacheIgnored);

        return D3D12_SHADER_BYTECODE
        {
            .pShaderBytecode = shaderDxil.data(),
            .BytecodeLength = shaderDxil.size(),
        };
    }

    static D3D12_RASTERIZER_DESC ToD3D12RasterizerState(const RasterizerState& rasterizerState)
    {
        return D3D12_RASTERIZER_DESC
        {
            .FillMode = (D3D12_FILL_MODE)rasterizerState.FillMode,
            .CullMode = (D3D12_CULL_MODE)rasterizerState.CullMode,
            .FrontCounterClockwise = rasterizerState.TriangleOrder == TriangleOrder::CounterClockwise,
            .DepthBias = rasterizerState.DepthBias,
            .DepthBiasClamp = rasterizerState.DepthBiasClamp,
            .SlopeScaledDepthBias = rasterizerState.SlopeScaledDepthBias,
            .DepthClipEnable = true,
            .MultisampleEnable = false,
            .AntialiasedLineEnable = false,
            .ForcedSampleCount = 0,
            .ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF,
        };
    }

    static D3D12_DEPTH_STENCIL_DESC ToD3D12DepthStencilState(const DepthState& depthState, const StencilState& stencilState)
    {
        return D3D12_DEPTH_STENCIL_DESC
        {
            .DepthEnable = depthState.IsEnabled,
            .DepthWriteMask = (D3D12_DEPTH_WRITE_MASK)depthState.IsWriteEnabled,
            .DepthFunc = (D3D12_COMPARISON_FUNC)depthState.ComparisonFunction,
            .StencilEnable = stencilState.IsEnabled,
            .StencilReadMask = stencilState.ReadMask,
            .StencilWriteMask = stencilState.WriteMask,
            .FrontFace
            {
                .StencilFailOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.StencilFailOperation,
                .StencilDepthFailOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.DepthFailOperation,
                .StencilPassOp = (D3D12_STENCIL_OP)stencilState.FrontFaceBehaviour.PassOperation,
                .StencilFunc = (D3D12_COMPARISON_FUNC)stencilState.FrontFaceBehaviour.StencilFunction,
            },
            .BackFace
            {
                .StencilFailOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.StencilFailOperation,
                .StencilDepthFailOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.DepthFailOperation,
                .StencilPassOp = (D3D12_STENCIL_OP)stencilState.BackFaceBehaviour.PassOperation,
                .StencilFunc = (D3D12_COMPARISON_FUNC)stencilState.BackFaceBehaviour.StencilFunction,
            },
        };
    }

    static D3D12_RENDER_TARGET_BLEND_DESC ToRenderTargetBlendDesc(const BlendState::RenderTargetState& blendRenderTargetState)
    {
        D3D12_RENDER_TARGET_BLEND_DESC d3d12RenderTargetBlendDesc
        {
            .RenderTargetWriteMask = blendRenderTargetState.ColorChannelFlags.GetRawBits(),
        };

        if (!blendRenderTargetState.IsEnabled)
        {
            d3d12RenderTargetBlendDesc.BlendEnable = false;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
        }
        else
        {
            d3d12RenderTargetBlendDesc.BlendEnable = true;
            d3d12RenderTargetBlendDesc.LogicOpEnable = false;
            d3d12RenderTargetBlendDesc.SrcBlend = (D3D12_BLEND)blendRenderTargetState.ColorEquation.SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlend = (D3D12_BLEND)blendRenderTargetState.ColorEquation.DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOp = (D3D12_BLEND_OP)blendRenderTargetState.ColorEquation.Operation;
            d3d12RenderTargetBlendDesc.SrcBlendAlpha = (D3D12_BLEND)blendRenderTargetState.AlphaEquation.SourceFactor;
            d3d12RenderTargetBlendDesc.DestBlendAlpha = (D3D12_BLEND)blendRenderTargetState.AlphaEquation.DestinationFactor;
            d3d12RenderTargetBlendDesc.BlendOpAlpha = (D3D12_BLEND_OP)blendRenderTargetState.AlphaEquation.Operation;
        }

        return d3d12RenderTargetBlendDesc;
    }

    static D3D12_BLEND_DESC ToD3D12BlendState(const BlendState& blendState)
    {
        D3D12_BLEND_DESC d3d12BlendDesc
        {
            .AlphaToCoverageEnable = blendState.IsAlphaToCoverageStateEnabled,
            .IndependentBlendEnable = blendState.IsIndependentBlendStateEnabled,
        };

        if (blendState.RenderTargetStates.empty())
        {
            d3d12BlendDesc.RenderTarget[0] = ToRenderTargetBlendDesc(BlendState::RenderTargetState{});
        }
        else
        {
            for (const auto [i, renderTargetState] : blendState.RenderTargetStates | std::views::enumerate)
            {
                d3d12BlendDesc.RenderTarget[i] = ToRenderTargetBlendDesc(renderTargetState);
            }
        }

        return d3d12BlendDesc;
    }

    // ShaderInfo

    ShaderInfo::ShaderInfo(ShaderType type, std::string_view fileName, std::string_view entryPoint, std::vector<std::string_view>&& defines)
        : m_Type{ type }
        , m_FileName{ fileName }
        , m_EntryPoint{ entryPoint }
        , m_Defines{ std::move(defines) }
        , m_Hash{ 0 }
    {
        if (m_EntryPoint.empty() && type != ShaderType::Library)
        {
            m_EntryPoint = g_DefaultShaderEntryPoints[+type];
        }

        m_Hash = HashCombine(m_Hash, +m_Type);
        m_Hash = HashCombine(m_Hash, m_FileName);
        m_Hash = HashCombine(m_Hash, m_EntryPoint);

        for (auto define : m_Defines)
        {
            m_Hash = HashCombine(m_Hash, define);
        }
    }

    // PipelineState

    PipelineState::PipelineState(Device& device, const PipelineStateCreationVariant& creation)
        : m_Device{ device }
        , m_CreationVariant{ creation }
    {
        m_CreationVariant | MakeVisitorMatch([this](const auto& creation)
        {
            StoreShaders(creation);
            Compile(creation, false);
        });
    }

    PipelineState::~PipelineState()
    {
        Reset();
    }

    ID3D12PipelineState* PipelineState::GetD3D12PipelineState() const
    {
        BenzinAssert(!IsRayTracing());
        return m_D3D12PipelineState;
    }

    ID3D12StateObject* PipelineState::GetD3D12StateObject() const
    {
        BenzinAssert(IsRayTracing());
        return m_D3D12StateObject;
    }

    bool PipelineState::IsRayTracing() const
    {
        return std::holds_alternative<RayTracingPipelineStateCreation>(m_CreationVariant);
    }

    bool PipelineState::Reload()
    {
        if (!IsAllShadersValid())
        {
            return false;
        }

        Reset();
        m_CreationVariant | MakeVisitorMatch([this](const auto& creation) { Compile(creation, true); });

        BenzinTrace("Pso '{}' reloaded", GetDxObjectDebugName(m_D3D12PipelineState));

        return true;
    }

    void PipelineState::StoreShaders(const GraphicsPipelineStateCreation& creation)
    {
        BenzinAssert(m_ShaderCount == 0);

        auto& nonConstCreation = const_cast<GraphicsPipelineStateCreation&>(creation);
        m_Shaders[0] = ShaderInfo{ ShaderType::Vertex, creation.VsFileName, creation.VsEntryPoint, std::move(nonConstCreation.VsDefines) };
        m_Shaders[1] = ShaderInfo{ ShaderType::Pixel, creation.PsFileName, creation.PsEntryPoint, std::move(nonConstCreation.PsDefines) };

        m_ShaderCount = 2;
    }

    void PipelineState::StoreShaders(const ComputePipelineStateCreation& creation)
    {
        BenzinAssert(m_ShaderCount == 0);

        auto& nonConstCreation = const_cast<ComputePipelineStateCreation&>(creation);
        m_Shaders[0] = ShaderInfo{ ShaderType::Compute, creation.CsFileName, creation.CsEntryPoint, std::move(nonConstCreation.CsDefines) };

        m_ShaderCount = 1;
    }

    void PipelineState::StoreShaders(const RayTracingPipelineStateCreation& creation)
    {
        BenzinAssert(m_ShaderCount == 0);

        auto& nonConstCreation = const_cast<RayTracingPipelineStateCreation&>(creation);
        m_Shaders[0] = ShaderInfo{ benzin::ShaderType::Library, creation.ShaderLibrary.FileName, {}, std::move(nonConstCreation.ShaderLibrary.Defines) };

        m_ShaderCount = 1;
    }

    void PipelineState::Compile(const GraphicsPipelineStateCreation& creation, bool isShaderCacheIgnored)
    {
        BenzinAssert(creation.RenderTargetFormats.size() <= 8);

        BenzinAssert(m_Shaders[0].GetType() == ShaderType::Vertex);
        BenzinAssert(m_Shaders[1].GetType() == ShaderType::Pixel);

        D3D12_GRAPHICS_PIPELINE_STATE_DESC d3d12GraphicsPipelineStateDesc
        {
            .pRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature(),
            .VS = ToD3D12Shader(m_Device, m_Shaders[0], isShaderCacheIgnored),
            .PS = ToD3D12Shader(m_Device, m_Shaders[1], isShaderCacheIgnored),
            .DS{ nullptr, 0 },
            .HS{ nullptr, 0 },
            .GS{ nullptr, 0 },
            .StreamOutput
            {
                .pSODeclaration = nullptr,
                .NumEntries = 0,
                .pBufferStrides = nullptr,
                .NumStrides = 0,
                .RasterizedStream = 0,
            },
            .BlendState = ToD3D12BlendState(creation.BlendState),
            .SampleMask = 0xffffffff,
            .RasterizerState = ToD3D12RasterizerState(creation.RasterizerState),
            .DepthStencilState = ToD3D12DepthStencilState(creation.DepthState, creation.StencilState),
            .InputLayout
            {
                .pInputElementDescs = nullptr,
                .NumElements = 0,
            },
            .IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED,
            .PrimitiveTopologyType = (D3D12_PRIMITIVE_TOPOLOGY_TYPE)creation.PrimitiveTopologyType,
            .NumRenderTargets = (UINT)creation.RenderTargetFormats.size(),
            .DSVFormat = (DXGI_FORMAT)creation.DepthStencilFormat,
            .SampleDesc{ 1, 0 },
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        memcpy(d3d12GraphicsPipelineStateDesc.RTVFormats, creation.RenderTargetFormats.data(), creation.RenderTargetFormats.size() * sizeof(GraphicsFormat));

        BenzinEnsure(m_Device.GetD3D12Device()->CreateGraphicsPipelineState(&d3d12GraphicsPipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDxObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

    void PipelineState::Compile(const ComputePipelineStateCreation& creation, bool isShaderCacheIgnored)
    {
        BenzinAssert(m_Shaders[0].GetType() == ShaderType::Compute);

        const D3D12_COMPUTE_PIPELINE_STATE_DESC d3d12ComputePipelineStateDesc
        {
            .pRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature(),
            .CS = ToD3D12Shader(m_Device, m_Shaders[0], isShaderCacheIgnored),
            .NodeMask = 0,
            .CachedPSO
            {
                .pCachedBlob = nullptr,
                .CachedBlobSizeInBytes = 0,
            },
            .Flags = D3D12_PIPELINE_STATE_FLAG_NONE,
        };

        BenzinEnsure(m_Device.GetD3D12Device()->CreateComputePipelineState(&d3d12ComputePipelineStateDesc, IID_PPV_ARGS(&m_D3D12PipelineState)));
        SetDxObjectDebugName(m_D3D12PipelineState, creation.DebugName);
    }

    void PipelineState::Compile(const RayTracingPipelineStateCreation& creation, bool isShaderCacheIgnored)
    {
        // D3D12_GLOBAL_ROOT_SIGNATURE
        const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
        {
            .pGlobalRootSignature = m_Device.GetUnifiedRootSignature().GetD3D12RootSignature(),
        };

        // D3D12_DXIL_LIBRARY_DESC
        const auto d3d12ShaderBytecode = ToD3D12Shader(m_Device, m_Shaders[0], isShaderCacheIgnored);

        const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
        {
            .DXILLibrary = d3d12ShaderBytecode,
            .NumExports = 0,
            .pExports = nullptr,
        };

        // D3D12_HIT_GROUP_DESC
        const std::wstring hitGroupName = ToWideString(creation.HitGroup.Name);
        const std::wstring closesHitEntryPoint = ToWideString(creation.HitGroup.ClosestHitEntryPoint);

        const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
        {
            .HitGroupExport = hitGroupName.data(),
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .AnyHitShaderImport = nullptr,
            .ClosestHitShaderImport = closesHitEntryPoint.data(),
            .IntersectionShaderImport = nullptr,
        };

        // D3D12_RAYTRACING_SHADER_CONFIG
        const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
        {
            .MaxPayloadSizeInBytes = std::max<uint32_t>(4u, creation.ShaderConfig.PayloadSize), // Min size is 4 bytes
            .MaxAttributeSizeInBytes = creation.ShaderConfig.AttributeSize, // Barycentrics
        };

        // D3D12_RAYTRACING_PIPELINE_CONFIG
        const D3D12_RAYTRACING_PIPELINE_CONFIG d3d12RaytracingPipelineConfig
        {
            .MaxTraceRecursionDepth = 1,
        };

        // Create ID3D12StateObject
        const auto d3d12StateSubObjects = std::to_array(
        {
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &d3d12GlobalRootSignature },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY, &d3d12DXILLibraryDesc },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &d3d12HitGroupDesc },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &d3d12RaytracingShaderConfig },
            D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &d3d12RaytracingPipelineConfig },
        });

        const D3D12_STATE_OBJECT_DESC d3d12StateObjectDesc
        {
            .Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE,
            .NumSubobjects = (uint32_t)d3d12StateSubObjects.size(),
            .pSubobjects = d3d12StateSubObjects.data(),
        };

        BenzinEnsure(m_Device.GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12StateObject)));
        SetDxObjectDebugName(m_D3D12StateObject, creation.DebugName);
    }

    bool PipelineState::IsAllShadersValid() const
    {
        auto& shaderManager = m_Device.GetBackend().GetShaderManager();

        return std::ranges::all_of(GetShaders(), [&shaderManager](const auto& shader)
        {
            return shaderManager.TryCompileShaderIfNeeded(shader);
        });
    }

    void PipelineState::Reset()
    {
        m_Device.DeferredRelease(*this);
        m_D3D12PipelineState = nullptr;
    }

}
