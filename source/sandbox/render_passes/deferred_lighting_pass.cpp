#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/deferred_lighting_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/deferred_lighting_resources.hpp>
#include <shaders/joint/full_screen_debug_resources.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_DeferredLighting);

namespace sandbox
{

    constexpr auto g_LightingFormat = benzin::GraphicsFormat::Rgba16Float;

    DeferredLightingPass::DeferredLightingPass()
    {
        ms_PsoManager->Create(PsoId::DeferredLighting, [this](benzin::GraphicsPsoProxy& proxy)
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
            proxy.RenderTargetFormats.push_back(g_LightingFormat);
        });
    }

    DeferredLightingPass::~DeferredLightingPass()
    {
        ms_PsoManager->Destroy(PsoId::DeferredLighting);
        ms_Resources->Destroy(TextureId::HdrColor);
    }

    void DeferredLightingPass::OnRenderViewportResize()
    {
        ms_Resources->Create(TextureId::HdrColor, benzin::TextureCreation
        {
            .DebugName = "DeferredLighting_HdrColor",
            .Format = g_LightingFormat,
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
        BenzinProfile();

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "DeferredLighting");

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& shadow = ms_Resources->Get(sigmaSettings.IsEnabled ? TextureId::Shadow : TextureId::NoisyPenumbra);
        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ hdrColor, benzin::ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ hdrColor.GetRtv() });
        commandList.ClearRenderTarget(hdrColor);

        commandList.SetGraphicsPso(ms_PsoManager->GetGraphics(PsoId::DeferredLighting));

        {
            using enum joint::Rc_DeferredLighting;

            commandList.SetGraphicsRootResource(+AlbedoAndRoughness, ms_Resources->Get(TextureId::AlbedoAndRoughness).GetSrv());
            commandList.SetGraphicsRootResource(+EmissiveAndMetallic, ms_Resources->Get(TextureId::EmissiveAndMetallic).GetSrv());
            commandList.SetGraphicsRootResource(+WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            commandList.SetGraphicsRootResource(+DepthStencil, ms_Resources->Get(TextureId::DepthStencil).GetSrv());
            commandList.SetGraphicsRootResource(+Shadow, shadow.GetSrv());
        }

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.DrawVertexed(3);
    }

}
