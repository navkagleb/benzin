#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/deferred_lighting_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

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

BenzinAllowDereferenceOperatorForEnum(joint::DeferredLightingResources);

namespace sandbox
{

    DeferredLightingPass::DeferredLightingPass()
    {
        ms_PsoManager->Create(PsoId::DeferredLighting, [this](benzin::VertexPsoProxy& proxy)
        {
            proxy.m_Vs.m_FileName = "fullscreen_triangle.hlsl";
            proxy.m_Ps.m_FileName = "deferred_lighting_pass.hlsl";
            proxy.m_RenderTargetDxgiFormats.push_back(DeferredLightingSettings::ms_HdrColorDxgiFormat);
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
            .m_DebugName = "DeferredLighting::HdrColor",
            .m_DxgiFormat = DeferredLightingSettings::ms_HdrColorDxgiFormat,
            .m_Width = ms_RenderViewportWidth,
            .m_Height = ms_RenderViewportHeight,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowRenderTarget,
        });
    }

    void DeferredLightingPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("DeferredLighting");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& shadow = ms_Resources->Get(sigmaSettings.m_IsEnabled ? TextureId::Shadow : TextureId::NoisyPenumbra);
        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ hdrColor, D3D12_RESOURCE_STATE_RENDER_TARGET });

        cmdList.SetRenderTargets({ hdrColor.GetRtv() });
        cmdList.ClearRenderTarget(hdrColor);

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::DeferredLighting));

        {
            using Resources = joint::DeferredLightingResources;

            cmdList.SetGraphicsRootResource(*Resources::AlbedoAndRoughness, ms_Resources->Get(TextureId::AlbedoAndRoughness).GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::EmissiveAndMetallic, ms_Resources->Get(TextureId::EmissiveAndMetallic).GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::DepthStencil, ms_Resources->Get(TextureId::DepthStencil).GetSrv());
            cmdList.SetGraphicsRootResource(*Resources::Shadow, shadow.GetSrv());
        }

        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cmdList.DrawVertexed(3);
    }

}
