#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/ray_tracing_shadow_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::RayTracing_ShadowResources);

namespace sandbox
{

    RayTracing_ShadowPass::RayTracing_ShadowPass()
    {
        ms_PsoManager->Create(PsoId::ShadowPass, [](benzin::RayTracing_PsoProxy& proxy)
        {
            proxy.m_ShaderLibrary.m_FileName = "ray_tracing_shadow_pass.hlsl";
            proxy.m_RayGenerationEntryPoint = "RayGeneration";
            proxy.m_MissEntryPoint = "Miss";
            proxy.m_HitGroup.m_Name = "HitGroup";
            proxy.m_HitGroup.m_ClosestHitEntryPoint = "ClosestHit";
            proxy.m_ShaderConfig.m_PayloadSizeInBytes = sizeof(joint::RayTracing_ShadowPayload);
            proxy.m_ShaderConfig.m_AttributeSizeInBytes = sizeof(DirectX::XMFLOAT2); // Barycentrics
        });
    }

    RayTracing_ShadowPass::~RayTracing_ShadowPass()
    {
        ms_PsoManager->Destroy(PsoId::ShadowPass);
        ms_Resources->Destroy(TextureId::NoisyPenumbra);
    }

    void RayTracing_ShadowPass::OnZeroFrameInit()
    {
        benzin::TextureImage blueNoiseImage;
        benzin::LoadTextureImageFromDdsFile("blue_noise_128_rgba_array.dds", blueNoiseImage);

        benzin::MakeUniquePtr(m_BlueNoiseTexture, *ms_Device, benzin::TextureCreation
        {
            .m_DebugName = "BlueNoise",
            .m_DxgiFormat = blueNoiseImage.m_DxgiFormat,
            .m_Width = blueNoiseImage.m_Width,
            .m_Height = blueNoiseImage.m_Height,
            .m_Depth = blueNoiseImage.m_Depth,
            .m_MipCount = 1,
        });

        benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_BlueNoiseTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*m_BlueNoiseTexture, benzin::ToSpan(blueNoiseImage.m_PixelData));

        auto& settings = ms_Settings->GetSection<RayTracing_ShadowSettings>();
        settings.m_BlueNoiseDepth = m_BlueNoiseTexture->GetDepth();
    }

    void RayTracing_ShadowPass::OnRenderViewportResize()
    {
        ms_Resources->Create(TextureId::NoisyPenumbra, benzin::TextureCreation
        {
            .m_DebugName = magic_enum::enum_name(TextureId::NoisyPenumbra),
            .m_DxgiFormat = SigmaDenoiserSettings::ms_PenumbraDxgiFormat,
            .m_Width = ms_RenderViewportWidth,
            .m_Height = ms_RenderViewportHeight,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    void RayTracing_ShadowPass::OnUpdate()
    {
        BenzinProfile();

        auto& settings = ms_Settings->GetSection<RayTracing_ShadowSettings>();

        m_Consts.m_IsShadowsEnabled = settings.m_IsEnabled;
        m_Consts.m_IsBlueNoiseUsed = settings.m_IsBlueNoiseUsed;
        m_Consts.m_IsNoiseAnimated = settings.m_IsNoiseAnimated;

        if (!settings.m_IsBlueNoiseDepthFreezed)
        {
            settings.m_BlueNoiseDepthIndex = (settings.m_BlueNoiseDepthIndex + 1) % settings.m_BlueNoiseDepth;
            m_BlueNoiseDepthIndex = settings.m_BlueNoiseDepthIndex;
        }

        if (settings.m_IsEnabled)
        {
            BenzinScopeProfile("UpdateTlas");
            ms_RayTracingScene->UpdateTlas();
        }
    }

    void RayTracing_ShadowPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Shadows");

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        if (m_Consts.m_IsShadowsEnabled)
        {
            BenzinScopeProfile("TlasBuilding");
            BenzinGpuProfile("TlasBuilding");

            const benzin::RayTracing_Tlas& tlas = ms_RayTracingScene->GetActiveTlas();

            cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *tlas.GetScratchResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS }, true);

            cmdList.BuildRayTracingAccelerationStructure(tlas);
            cmdList.SetComputeSrv(benzin::UnifiedRootParameter::SceneTlas, tlas.GetGpuVirtualAddress());

            cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *tlas.GetScratchResource(), D3D12_RESOURCE_STATE_COMMON });
            cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ *tlas.GetScratchResource() }, true);
        }

        {
            BenzinScopeProfile("RayTracing");
            BenzinGpuProfile("RayTracing");

            const benzin::RayTracing_Pso& pso = ms_PsoManager->GetRayTracing(PsoId::ShadowPass);
            const benzin::Texture& noisyPenumbra = ms_Resources->Get(TextureId::NoisyPenumbra);

            cmdList.SetRayTracingPso(pso);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

            cmdList.AddResourceBarrier(benzin::TransitionBarrier{ noisyPenumbra, D3D12_RESOURCE_STATE_UNORDERED_ACCESS }, true);

            using Resources = joint::RayTracing_ShadowResources;
            cmdList.SetComputeRootResource(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(*Resources::Depth, ms_Resources->Get(TextureId::DepthStencil).GetSrv());
            cmdList.SetComputeRootResource(*Resources::BlueNoise, m_BlueNoiseTexture->GetSrv({ .m_DepthOffset = m_BlueNoiseDepthIndex, .m_DepthCount = 1 }));
            cmdList.SetComputeRootResource(*Resources::NoisyPenumbra, noisyPenumbra.GetUav());

            cmdList.DispatchRays(pso.GetShaderTable(), { ms_RenderViewportWidth, ms_RenderViewportHeight, 1 });

            cmdList.AddResourceBarrier(benzin::TransitionBarrier{ noisyPenumbra, D3D12_RESOURCE_STATE_COMMON });
            cmdList.AddResourceBarrier(benzin::UnorderedAccessBarrier{ noisyPenumbra }, true);
        }
    }

}
