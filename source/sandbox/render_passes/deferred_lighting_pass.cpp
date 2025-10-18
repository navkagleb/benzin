#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/deferred_lighting_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <shaders/joint/deferred_lighting_resources.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::DeferredLightingResources);

namespace sandbox
{

    DeferredLightingPass::DeferredLightingPass()
    {
        ms_PsoManager->Create(PsoId::DeferredLighting, [this](benzin::VertexPsoProxy& proxy)
        {
            proxy.Vs.FileName = "fullscreen_triangle.hlsl";
            proxy.Ps.FileName = "deferred_lighting_pass.hlsl";
            proxy.DepthState = benzin::DepthState
            {
                .IsEnabled = false,
                .IsWriteEnabled = false,
            };
            proxy.RenderTargetFormats.push_back(DeferredLightingSettings::s_HdrColorFormat);
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
            .DebugName = "DeferredLighting::HdrColor",
            .Format = DeferredLightingSettings::s_HdrColorFormat,
            .Width = ms_RenderViewportWidth,
            .Height = ms_RenderViewportHeight,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget,
        });
    }

    void DeferredLightingPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("DeferredLighting");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& shadow = ms_Resources->Get(sigmaSettings.IsEnabled ? TextureId::Shadow : TextureId::NoisyPenumbra);
        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ hdrColor, benzin::ResourceState::RenderTarget });

        cmdList.SetRenderTargets({ hdrColor.GetRtv() });
        cmdList.ClearRenderTarget(hdrColor);

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::DeferredLighting));

        {
            using enum joint::DeferredLightingResources;

            cmdList.SetGraphicsRootResource(+AlbedoAndRoughness, ms_Resources->Get(TextureId::AlbedoAndRoughness).GetSrv());
            cmdList.SetGraphicsRootResource(+EmissiveAndMetallic, ms_Resources->Get(TextureId::EmissiveAndMetallic).GetSrv());
            cmdList.SetGraphicsRootResource(+WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetGraphicsRootResource(+DepthStencil, ms_Resources->Get(TextureId::DepthStencil).GetSrv());
            cmdList.SetGraphicsRootResource(+Shadow, shadow.GetSrv());
        }

        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList.DrawVertexed(3);
    }

}
