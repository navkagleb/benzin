#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_tracing_shadows_pass.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <shaders/joint/root_constants.hpp>
#include <shaders/joint/structured_buffer_types.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    static constexpr auto g_RayGenShaderName = L"RayGen"sv;
    static constexpr auto g_MissShaderName = L"Miss"sv;

    static constexpr auto g_HitGroupName = L"HitGroup"sv;
    static constexpr auto g_ClosestHitShaderName = L"ClosestHitShader"sv;

    //

    RayTracingShadowsPass::RayTracingShadowsPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        CreatePipelineStateObject();
        CreateShaderTable();

        benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "RayTracingShadowsConstants");
    }

    RayTracingShadowsPass::~RayTracingShadowsPass()
    {
        ms_Resources->DestroyTexture(+Texture::NoisyPenumbra);
    }

    void RayTracingShadowsPass::OnRenderViewportResize()
    {
        ms_Resources->CreateTexture(+Texture::NoisyPenumbra, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::NoisyPenumbra),
            .Format = benzin::GraphicsFormat::R16Float,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    void RayTracingShadowsPass::OnUpdate()
    {
        const auto& perspectiveProjection = m_Scene.GetPerspectiveProjection();

        const auto& rayTracingSettings = ms_Settings->GetSection<RayTracingShadowsSettings>();
        const auto& deferredLightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        const float pixelAngularRadiusInRadians = 0.5f * perspectiveProjection.GetVerticalFovInRadians() / (float)GetRenderViewportWidth();
        const float sunAngularRadiusInRadians = 0.5f * deferredLightingSettings.SunAngularDiameterInRadians;

        m_PassConstantBuffer->UpdateConstants(joint::RayTracingShadowsConstants
        {
            .RaysPerPixel = rayTracingSettings.RaysPerPixel,
            .TanSunAngularRadius = std::tan(sunAngularRadiusInRadians),
            .PixelAngularRadiusInRadians = pixelAngularRadiusInRadians,
            .SunAngularRadiusInRadians = sunAngularRadiusInRadians,
            .SunDirection = GetSunDirection(deferredLightingSettings),
        });
    }

    void RayTracingShadowsPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();
        BenzinPushGpuEvent(commandList, "RayTracingShadowPass");

        const auto& noisyPenumbra = ms_Resources->GetTexture(+Texture::NoisyPenumbra);

        d3d12CommandList->SetPipelineState1(m_D3D12RaytracingStateObject.Get());

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
        commandList.SetRootResource(joint::RayTracingShadowsRc_WorldNormalTex, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
        commandList.SetRootResource(joint::RayTracingShadowsRc_DepthTex, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
        commandList.SetRootResource(joint::RayTracingShadowsRc_OutNoisyPenumbraTex, noisyPenumbra.GetUav());

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ noisyPenumbra, benzin::ResourceState::UnorderedAccess },
        );

        const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
        {
            .RayGenerationShaderRecord
            {
                .StartAddress = m_RayGenShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_RayGenShaderTable->GetNotAlignedSize(),
            },
            .MissShaderTable
            {
                .StartAddress = m_MissShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_MissShaderTable->GetNotAlignedSize(),
                .StrideInBytes = m_MissShaderTable->GetElementSize(),
            },
            .HitGroupTable
            {
                .StartAddress = m_HitGroupShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSize(),
                .StrideInBytes = m_HitGroupShaderTable->GetElementSize(),
            },
            .CallableShaderTable
            {
                .StartAddress = 0,
                .SizeInBytes = 0,
                .StrideInBytes = 0,
            },
            .Width = noisyPenumbra.GetWidth(),
            .Height = noisyPenumbra.GetHeight(),
            .Depth = 1,
        };

        d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
    }

    void RayTracingShadowsPass::CreatePipelineStateObject()
    {
        // 1. D3D12_GLOBAL_ROOT_SIGNATURE
        const D3D12_GLOBAL_ROOT_SIGNATURE d3d12GlobalRootSignature
        {
            .pGlobalRootSignature = ms_Device->GetUnifiedRootSignature().GetD3D12RootSignature(),
        };

        // 2. D3D12_DXIL_LIBRARY_DESC
        const benzin::ShaderInfo shaderLibrary{ benzin::ShaderType::Library, "ray_tracing_shadows_pass.hlsl", {}, {} };
        const std::span libraryDxil = ms_Device->GetBackend().GetShaderManager().GetShaderDxil(shaderLibrary);

        const D3D12_DXIL_LIBRARY_DESC d3d12DXILLibraryDesc
        {
            .DXILLibrary
            {
                .pShaderBytecode = libraryDxil.data(),
                .BytecodeLength = libraryDxil.size(),
            },
            .NumExports = 0,
            .pExports = nullptr,
        };

        // 3. D3D12_HIT_GROUP_DESC
        const D3D12_HIT_GROUP_DESC d3d12HitGroupDesc
        {
            .HitGroupExport = g_HitGroupName.data(),
            .Type = D3D12_HIT_GROUP_TYPE_TRIANGLES,
            .AnyHitShaderImport = nullptr,
            .ClosestHitShaderImport = g_ClosestHitShaderName.data(),
            .IntersectionShaderImport = nullptr,
        };

        // 4. D3D12_RAYTRACING_SHADER_CONFIG
        const D3D12_RAYTRACING_SHADER_CONFIG d3d12RaytracingShaderConfig
        {
            .MaxPayloadSizeInBytes = std::max<uint32_t>(4, sizeof(joint::ShadowRayPayload)), // #TODO: Min size is 4 bytes
            .MaxAttributeSizeInBytes = sizeof(DirectX::XMFLOAT2), // Barycentrics
        };

        // 5. D3D12_RAYTRACING_PIPELINE_CONFIG
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

        BenzinEnsure(ms_Device->GetD3D12Device()->CreateStateObject(&d3d12StateObjectDesc, IID_PPV_ARGS(&m_D3D12RaytracingStateObject)));
    }

    void RayTracingShadowsPass::CreateShaderTable()
    {
        BenzinEnsure(m_D3D12RaytracingStateObject.Get());

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinEnsure(m_D3D12RaytracingStateObject.As(&d3d12StateObjectProperties));

        const auto CreateShaderTable = [&](std::wstring_view identiferName)
        {
            const void* rawShaderIdentifier = d3d12StateObjectProperties->GetShaderIdentifier(identiferName.data());
            const auto shaderIdentifier = std::span{ (const std::byte*)rawShaderIdentifier, benzin::GfxConfig::s_ShaderIdentifierSize };

            auto shaderTableBuffer = std::make_unique<benzin::Buffer>(*ms_Device, benzin::BufferCreation
            {
                .DebugName = std::format("{}ShaderTable", benzin::ToNarrowString(identiferName)),
                .MemoryType = benzin::ResourceMemoryType::Upload,
                .ElementSize = benzin::GfxConfig::s_RayTracingShaderRecordAlignment,
                .ElementCount = 1,
            });

            const benzin::MemoryWriter shaderTableWriter{ shaderTableBuffer->GetCpuMappedData(), shaderTableBuffer->GetSize() };
            shaderTableWriter.WriteBytes(shaderIdentifier);

            return shaderTableBuffer;
        };

        m_RayGenShaderTable = CreateShaderTable(g_RayGenShaderName);
        m_MissShaderTable = CreateShaderTable(g_MissShaderName);
        m_HitGroupShaderTable = CreateShaderTable(g_HitGroupName);
    }

}
