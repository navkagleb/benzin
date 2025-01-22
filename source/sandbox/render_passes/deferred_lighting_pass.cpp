#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/deferred_lighting_pass.hpp"

#include <benzin/core/command_line_args.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/deferred_lighting_resources.hpp>
#include <shaders/joint/full_screen_debug_resources.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_DeferredLighting);

namespace sandbox
{

    DeferredLightingPass::DeferredLightingPass()
        : m_RenderTargetFormat{ (benzin::GraphicsFormat)benzin::CommandLineArgs::GetU32("BackBufferFormat") }
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
        const auto& fullScreenDebugSettings = ms_Settings->GetSection<FullScreenDebugSettings>();

        m_IsRenderingEnabled = fullScreenDebugSettings.DebugOutputType == joint::DebugOutputType::None;
    }

    void DeferredLightingPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "DeferredLightingPass");

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& shadow = ms_Resources->GetTexture(sigmaSettings.IsEnabled ? +Texture::Shadow : +Texture::NoisyPenumbra);
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

        {
            using enum joint::Rc_DeferredLighting;

            commandList.SetRootResource(+AlbedoAndRoughness, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(+EmissiveAndMetallic, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+DepthStencil, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
            commandList.SetRootResource(+Shadow, shadow.GetSrv());
        }

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

}
