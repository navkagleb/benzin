#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/ray_tracing_shader_table.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_RayTracing_Shadow);

namespace sandbox
{

    RayTracing_ShadowPass::RayTracing_ShadowPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        ms_PsoManager->CreateRayTracingPso(+Pso::ShadowPass, [](benzin::RayTracing_PsoProxy& proxy)
        {
            proxy.DebugName = "RayTracing_ShadowPass";
            proxy.ShaderLibrary.FileName = "ray_tracing_shadow_pass.hlsl";
            proxy.RayGenerationEntryPoint = "RayGeneration";
            proxy.MissEntryPoint = "Miss";
            proxy.HitGroup.Name = "HitGroup";
            proxy.HitGroup.ClosestHitEntryPoint = "ClosestHit";
            proxy.ShaderConfig.PayloadSize = sizeof(joint::RayTracing_ShadowPayload);
            proxy.ShaderConfig.AttributeSize = sizeof(DirectX::XMFLOAT2); // Barycentrics
        });

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
    }

    RayTracing_ShadowPass::~RayTracing_ShadowPass()
    {
        ms_PsoManager->DestroyPso(+Pso::ShadowPass);
        ms_Resources->DestroyTexture(+Texture::NoisyPenumbra);
    }

    void RayTracing_ShadowPass::OnZeroFrameInit()
    {
        benzin::TextureImage blueNoiseImage;
        benzin::LoadTextureImageFromDdsFile("blue_noise_64.dds", blueNoiseImage);

        benzin::MakeUniquePtr(m_BlueNoise, *ms_Device, benzin::TextureCreation
        {
            .DebugName = "BlueNoise64",
            .Format = blueNoiseImage.Format,
            .Width = blueNoiseImage.Width,
            .Height = blueNoiseImage.Height,
            .MipCount = 1,
        });

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(m_BlueNoise->GetSize());
        commandList.UploadToTextureTopMip(*m_BlueNoise, std::as_bytes(std::span{ blueNoiseImage.ImageData }));
    }

    void RayTracing_ShadowPass::OnRenderViewportResize()
    {
        const auto penumbraFormat = ms_Settings->GetSection<SigmaDenoiserSettings>().PenumbraFormat;

        ms_Resources->CreateTexture(+Texture::NoisyPenumbra, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::NoisyPenumbra),
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
        const auto& shadowSettings = ms_Settings->GetSection<RayTracing_ShadowSettings>();

        m_Consts.IsBlueNoiseUsed = shadowSettings.IsBlueNoiseUsed;
        m_Consts.IsNoiseAnimated = shadowSettings.IsNoiseAnimated;
    }

    void RayTracing_ShadowPass::OnRender() const
    {
        BenzinProfile();

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "RayTracing_Shadow");

        const auto& pso = ms_PsoManager->GetRayTracingPso(+Pso::ShadowPass);
        const auto& noisyPenumbra = ms_Resources->GetTexture(+Texture::NoisyPenumbra);

        commandList.SetPso(pso);
        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        {
            using enum joint::Rc_RayTracing_Shadow;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+Depth, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
            commandList.SetRootResource(+BlueNoise, m_BlueNoise->GetSrv());

            commandList.SetRootResource(+OutNoisyPenumbra, noisyPenumbra.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ noisyPenumbra, benzin::ResourceState::UnorderedAccess },
        );

        commandList.DispatchRays(pso.GetShaderTable(), { GetRenderViewportWidth(), GetRenderViewportHeight(), 1 });
    }

}
