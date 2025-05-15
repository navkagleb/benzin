#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/ray_tracing_shader_table.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include "sandbox/render_passes/sigma_denoiser_pass.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::RayTracing_ShadowResources);

namespace sandbox
{

    RayTracing_ShadowPass::RayTracing_ShadowPass()
    {
        ms_PsoManager->Create(PsoId::ShadowPass, [](benzin::RayTracing_PsoProxy& proxy)
        {
            proxy.ShaderLibrary.FileName = "ray_tracing_shadow_pass.hlsl";
            proxy.RayGenerationEntryPoint = "RayGeneration";
            proxy.MissEntryPoint = "Miss";
            proxy.HitGroup.Name = "HitGroup";
            proxy.HitGroup.ClosestHitEntryPoint = "ClosestHit";
            proxy.ShaderConfig.PayloadSizeInBytes = sizeof(joint::RayTracing_ShadowPayload);
            proxy.ShaderConfig.AttributeSizeInBytes = sizeof(DirectX::XMFLOAT2); // Barycentrics
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
            .DebugName = "BlueNoise",
            .Format = blueNoiseImage.Format,
            .Width = blueNoiseImage.Width,
            .Height = blueNoiseImage.Height,
            .Depth = blueNoiseImage.Depth,
            .MipCount = 1,
        });

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_BlueNoiseTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*m_BlueNoiseTexture, benzin::ToSpan(blueNoiseImage.PixelData));

        auto& settings = ms_Settings->GetSection<RayTracing_ShadowSettings>();
        settings.BlueNoiseDepth = m_BlueNoiseTexture->GetDepth();
    }

    void RayTracing_ShadowPass::OnRenderViewportResize()
    {
        const auto penumbraFormat = ms_Settings->GetSection<SigmaDenoiserSettings>().PenumbraFormat;

        ms_Resources->Create(TextureId::NoisyPenumbra, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(TextureId::NoisyPenumbra),
            .Format = penumbraFormat,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .Depth = benzin::Scene::s_MaxLightCount,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    void RayTracing_ShadowPass::OnUpdate()
    {
        auto& settings = ms_Settings->GetSection<RayTracing_ShadowSettings>();

        m_Consts.IsBlueNoiseUsed = settings.IsBlueNoiseUsed;
        m_Consts.IsNoiseAnimated = settings.IsNoiseAnimated;

        if (!settings.IsBlueNoiseDepthFreezed)
        {
            settings.BlueNoiseDepthIndex = (settings.BlueNoiseDepthIndex + 1) % settings.BlueNoiseDepth;
            m_BlueNoiseDepthIndex = settings.BlueNoiseDepthIndex;
        }

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
    }

    void RayTracing_ShadowPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "RayTracing_Shadow");

        const auto& pso = ms_PsoManager->GetRayTracing(PsoId::ShadowPass);
        const auto& noisyPenumbra = ms_Resources->Get(TextureId::NoisyPenumbra);

        cmdList.SetRayTracingPso(pso);
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        {
            using enum joint::RayTracing_ShadowResources;

            cmdList.SetComputeRootResource(+WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(+Depth, ms_Resources->Get(TextureId::DepthStencil).GetSrv());
            cmdList.SetComputeRootResource(+BlueNoise, m_BlueNoiseTexture->GetSrv({ .DepthRange = m_BlueNoiseDepthIndex }));

            cmdList.SetComputeRootResource(+OutNoisyPenumbra, noisyPenumbra.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ noisyPenumbra, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.DispatchRays(pso.GetShaderTable(), { GetRenderViewportWidth(), GetRenderViewportHeight(), 1 });
    }

}
