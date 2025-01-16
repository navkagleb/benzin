#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics/ray_tracing_shader_table.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/ray_tracing_shadow_resources.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

namespace sandbox
{

    static void BuildOrthonormalBasis(DirectX::XMFLOAT3 normal3, DirectX::XMFLOAT3& outTangent3, DirectX::XMFLOAT3& outBitangent3)
    {
        const DirectX::XMVECTOR up = abs(normal3.y) < 0.9999f ? DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f } : DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f };
        const DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&normal3);

        const DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, normal));
        const DirectX::XMVECTOR bitangent = DirectX::XMVector3Cross(normal, tangent);

        DirectX::XMStoreFloat3(&outTangent3, tangent);
        DirectX::XMStoreFloat3(&outBitangent3, bitangent);
    }

    //

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

        benzin::MakeUniquePtr(m_PassConstBuffer, *ms_Device, "RayTracing_ShadowConsts");
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
        ms_Resources->CreateTexture(+Texture::NoisyPenumbra, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::NoisyPenumbra),
            .Format = SigmaDenoiserPass::s_PenumbraFormat,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    void RayTracing_ShadowPass::OnUpdate()
    {
        const auto& shadowSettings = ms_Settings->GetSection<RayTracing_ShadowSettings>();
        const auto& lightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        const float sunAngularRadiusInRadians = 0.5f * lightingSettings.SunAngularDiameterInRadians;
        const DirectX::XMFLOAT3 toSunDirection = GetSunDirection(lightingSettings);

        DirectX::XMFLOAT3 toSunTangent;
        DirectX::XMFLOAT3 toSunBitangent;
        BuildOrthonormalBasis(toSunDirection, toSunTangent, toSunBitangent);

        const auto& localLight = m_Scene.GetEntityRegistry().get<benzin::TransformComponent>(shadowSettings.LightHandle);

        m_PassConstBuffer->UpdateConstants(joint::RayTracing_ShadowConsts
        {
            .ToSunDirection = toSunDirection,
            .TanSunAngularRadius = std::tan(sunAngularRadiusInRadians),
            .ToSunTangent = toSunTangent,
            .SunAngularRadiusInRadians = sunAngularRadiusInRadians,
            .ToSunBitangent = toSunBitangent,

            .LightPosition = localLight.GetTranslation(),
            .LightRadius = localLight.GetScale().x * 0.5f,

            .IsShadowsFromSun = shadowSettings.IsShadowsFromSun,
            .IsBlueNoiseUsed = shadowSettings.IsBlueNoiseUsed,
            .IsNoiseAnimated = shadowSettings.IsNoiseAnimated,
        });
    }

    void RayTracing_ShadowPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "RayTracingShadowPass");

        const auto& pso = ms_PsoManager->GetRayTracingPso(+Pso::ShadowPass);
        const auto& noisyPenumbra = ms_Resources->GetTexture(+Texture::NoisyPenumbra);

        commandList.SetPso(pso);
        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstBuffer->GetActiveGpuVirtualAddress());

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
