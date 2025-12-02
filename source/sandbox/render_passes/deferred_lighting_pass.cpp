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
            proxy.m_Vs.m_FileName = "deferred_lighting_pass.hlsl";
            proxy.m_Ps.m_FileName = "deferred_lighting_pass.hlsl";

            proxy.m_DepthState.m_IsEnabled = true;
            proxy.m_DepthState.m_IsWriteEnabled = false;
            proxy.m_DepthState.m_D3D12ComparisonFunction = D3D12_COMPARISON_FUNC_NOT_EQUAL;

            proxy.m_RenderTargetDxgiFormats.push_back(DeferredLightingSettings::ms_HdrColorDxgiFormat);
            proxy.m_DepthStencilDxgiFormat = GBufferSettings::ms_DepthStencilDxgiFormat;
        });
    }

    DeferredLightingPass::~DeferredLightingPass()
    {
        ms_PsoManager->Destroy(PsoId::DeferredLighting);
        ms_Resources->Destroy(TextureId::HdrColor);
    }

    void DeferredLightingPass::OnRenderViewportResize()
    {
        ms_Resources->Create(
            TextureId::HdrColor,
            DeferredLightingSettings::ms_HdrColorDxgiFormat,
            benzin::TextureAccessFlag::AllowRenderTarget);
    }

    void DeferredLightingPass::OnRender() const
    {
        using Resources = joint::DeferredLightingResources;

        BenzinProfile();
        BenzinGpuProfile("DeferredLighting");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);
        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::DeferredLighting));

        const auto& depth = ms_Resources->Get(TextureId::DepthStencil);
        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);

        cmdList.AddRenderTarget(hdrColor);
        cmdList.AddDepthStencil(depth, D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        cmdList.SetRenderTargets();

        cmdList.SetGraphicsRootSrv(*Resources::AlbedoAndRoughness, ms_Resources->Get(TextureId::AlbedoAndRoughness));
        cmdList.SetGraphicsRootSrv(*Resources::EmissiveAndMetallic, ms_Resources->Get(TextureId::EmissiveAndMetallic));
        cmdList.SetGraphicsRootSrv(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal));
        cmdList.SetGraphicsRootSrv(*Resources::DepthStencil, depth);

        const TextureId shadowId = ms_Settings->GetSection<SigmaDenoiserSettings>().m_IsEnabled ? TextureId::Shadow : TextureId::NoisyPenumbra;
        cmdList.SetGraphicsRootSrv(*Resources::Shadow, ms_Resources->Get(shadowId));

        cmdList.FlushBarriers();
        cmdList.ClearRenderTarget(hdrColor);
        cmdList.DrawVertexed(3);
    }

}
