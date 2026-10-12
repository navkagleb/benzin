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

BenzinAllowDereferenceOperatorForEnum(joint::RayTracingShadowResources);

namespace sandbox
{

    RayTracingShadowPass::RayTracingShadowPass()
    {
        ms_PsoManager->Create(PsoId::ShadowPass, [](benzin::RayTracing_PsoProxy& proxy)
        {
            proxy.m_ShaderLibrary.m_FileName = "ray_tracing_shadow_pass.hlsl";
            proxy.m_RayGenerationEntryPoint = "RayGeneration";
            proxy.m_MissEntryPoint = "Miss";
            proxy.m_HitGroup.m_Name = "HitGroup";
            proxy.m_HitGroup.m_ClosestHitEntryPoint = "ClosestHit";
            proxy.m_ShaderConfig.m_PayloadSizeInBytes = sizeof(joint::RayTracingShadowPayload);
            proxy.m_ShaderConfig.m_AttributeSizeInBytes = sizeof(DirectX::XMFLOAT2); // Barycentrics
        });
    }

    RayTracingShadowPass::~RayTracingShadowPass()
    {
        ms_PsoManager->Destroy(PsoId::ShadowPass);
        ms_Resources->Destroy(TextureId::NoisyPenumbra);
    }

    void RayTracingShadowPass::OnZeroFrameInit()
    {
        auto& settings = ms_Settings->GetSection<RayTracingShadowSettings>();
        if (!settings.m_IsAllowed)
            return;

        ms_RayTracingScene->BuildBlases(*ms_Device);

        benzin::TextureImage blueNoiseImage;
        benzin::LoadTextureImageFromDdsFile("blue_noise_128_rgba_array.dds", blueNoiseImage);

        m_BlueNoiseTexture = ms_Device->GetPersistentDefaultAllocator().AllocateTexture([&blueNoiseImage](benzin::TextureCreation& creation)
        {
            creation.m_DebugName = "BlueNoise";
            creation.m_DxgiFormat = blueNoiseImage.m_DxgiFormat;
            creation.m_Width = blueNoiseImage.m_Width;
            creation.m_Height = blueNoiseImage.m_Height;
            creation.m_Depth = blueNoiseImage.m_Depth;
            creation.m_MipCount = 1;
        });

        benzin::CopyCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_BlueNoiseTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*m_BlueNoiseTexture, benzin::ToSpan(blueNoiseImage.m_PixelData));

        settings.m_BlueNoiseDepth = m_BlueNoiseTexture->GetDepth();
    }

    void RayTracingShadowPass::OnRenderViewportResize()
    {
        ms_Resources->Create(
            TextureId::NoisyPenumbra,
            SigmaDenoiserSettings::ms_PenumbraDxgiFormat,
            benzin::TextureAccessFlag::AllowUnorderedAccess);
    }

    void RayTracingShadowPass::OnUpdate()
    {
        BenzinProfile();

        auto& settings = ms_Settings->GetSection<RayTracingShadowSettings>();

        m_IsRenderingEnabled = settings.m_IsAllowed;
        if (!m_IsRenderingEnabled)
            return;

        m_Consts.m_IsShadowsEnabled = settings.m_IsEnabled;
        m_Consts.m_IsBlueNoiseUsed = settings.m_IsBlueNoiseUsed;
        m_Consts.m_IsNoiseAnimated = settings.m_IsNoiseAnimated;

        if (!settings.m_IsBlueNoiseDepthFreezed)
        {
            settings.m_BlueNoiseDepthIndex = (settings.m_BlueNoiseDepthIndex + 1) % settings.m_BlueNoiseDepth;
        }

        if (settings.m_IsEnabled)
        {
            BenzinScopeProfile("UpdateTlasInstances");
            ms_RayTracingScene->UpdateTlasInstances(*ms_Device);
        }
    }

    void RayTracingShadowPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("Shadows");

        const auto& settings = ms_Settings->GetSection<RayTracingShadowSettings>();

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        if (settings.m_IsEnabled)
        {
            BenzinScopeProfile("TlasBuilding");
            BenzinGpuProfile("TlasBuilding");

            const benzin::RayTracing_Tlas& tlas = ms_RayTracingScene->GetTlas();

            cmdList.AddTransitionBarrier(*tlas.GetScratchResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cmdList.FlushBarriers();
            cmdList.BuildRayTracingAccelerationStructure(tlas);

            cmdList.AddUavBarrier(*tlas.GetBuffer());
            cmdList.FlushBarriers();

            cmdList.SetComputeSrv(benzin::UnifiedRootParameter::SceneTlas, tlas.GetGpuVirtualAddress());
        }

        {
            using Resources = joint::RayTracingShadowResources;

            BenzinScopeProfile("RayTracing");
            BenzinGpuProfile("RayTracing");

            const benzin::RayTracing_Pso& pso = ms_PsoManager->GetRayTracing(PsoId::ShadowPass);
            const benzin::Texture& noisyPenumbra = ms_Resources->Get(TextureId::NoisyPenumbra);

            cmdList.SetRayTracingPso(pso);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

            cmdList.SetComputeRootSrv(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal));
            cmdList.SetComputeRootSrv(*Resources::Depth, ms_Resources->Get(TextureId::Depth));
            cmdList.SetComputeRootSrv(*Resources::BlueNoise, *m_BlueNoiseTexture, { .m_DepthOffset = settings.m_BlueNoiseDepthIndex, .m_DepthCount = 1 });
            cmdList.SetComputeRootUav(*Resources::NoisyPenumbra, noisyPenumbra);
            cmdList.FlushBarriers();

            cmdList.DispatchRays(pso.GetShaderTable(), { ms_RenderViewportWidth, ms_RenderViewportHeight, 1 });

            cmdList.AddUavBarrier(noisyPenumbra);
        }
    }

}
