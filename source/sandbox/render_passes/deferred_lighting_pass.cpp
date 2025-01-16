#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/deferred_lighting_pass.hpp"

#include <benzin/engine/entity_components.hpp> // TODO: Remove
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/constant_buffer_types.hpp>
#include <shaders/joint/root_constants.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    DeferredLightingPass::DeferredLightingPass(const benzin::Scene& scene)
        : m_Scene{ scene }
        , m_RenderTargetFormat{ (benzin::GraphicsFormat)benzin::CommandLineArgs::GetU32("BackBufferFormat") }
    {
        ms_PsoManager->CreateGraphicsPso(+Pso::DeferredLighting, [this](benzin::GraphicsPsoProxy& proxy)
        {
            proxy.DebugName = "DeferredLightingPass";
            proxy.VsFileName = "fullscreen_triangle.hlsl";
            proxy.PsFileName = "deferred_lighting_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            proxy.DepthState = benzin::DepthState
            {
                .IsEnabled = false,
                .IsWriteEnabled = false,
            };
            proxy.RenderTargetFormats.push_back(m_RenderTargetFormat);
        });

        benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "DeferredLightingPassConstantBuffer");
    }

    DeferredLightingPass::~DeferredLightingPass()
    {
        ms_PsoManager->DestroyPso(+Pso::DeferredLighting);

        ms_Resources->DestroyTexture(+Texture::Final);
    }

    void DeferredLightingPass::OnRenderViewportResize()
    {
        ms_Resources->CreateTexture(+Texture::Final, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::Final),
            .Format = m_RenderTargetFormat,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget,
        });
    }

    void DeferredLightingPass::OnUpdate()
    {
        const auto& deferredLightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();
        const auto& fullScreenDebugSettings = ms_Settings->GetSection<FullScreenDebugSettings>();

        m_IsRenderingEnabled = fullScreenDebugSettings.DebugOutputType == joint::DebugOutputType_None;

        m_PassConstantBuffer->UpdateConstants(joint::DeferredLightingPassConstants
        {
            .SunColor = deferredLightingSettings.SunColor,
            .SunIntensity = deferredLightingSettings.SunIntensity,
            .SunDirection = GetSunDirection(deferredLightingSettings),
            .ActivePointLightCount = (uint32_t)m_Scene.GetEntityRegistry().view<benzin::PointLightComponent>().size(), // TODO
        });
    }

    void DeferredLightingPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "DeferredLightingPass");

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& shadows = ms_Resources->GetTexture(sigmaSettings.IsEnabled ? +Texture::Shadow : +Texture::NoisyPenumbra);
        const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ finalTexture.GetRtv() });
        commandList.ClearRenderTarget(finalTexture);

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::DeferredLighting));

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
        commandList.SetRootResource(joint::DeferredLightingPassRc_AlbedoAndRoughnessTex, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_EmissiveAndMetallicTex, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_WorldNormalTex, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_VelocityTex, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_DepthStencilTex, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_PointLightBuf, m_Scene.GetPointLightBufferStructuredSrv());
        commandList.SetRootResource(joint::DeferredLightingPassRc_SigmaShadowTex, shadows.GetSrv());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

}
