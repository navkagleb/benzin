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
        using Resources = joint::DeferredLightingResources;

        BenzinProfile();
        BenzinGpuProfile("DeferredLighting");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const auto& albedoAndRoughness = ms_Resources->Get(TextureId::AlbedoAndRoughness);
        const auto& emissiveAndMetallic = ms_Resources->Get(TextureId::EmissiveAndMetallic);
        const auto& worldNormal = ms_Resources->Get(TextureId::WorldNormal);
        const auto& depth = ms_Resources->Get(TextureId::DepthStencil);
        const auto& shadow = ms_Resources->Get(sigmaSettings.m_IsEnabled ? TextureId::Shadow : TextureId::NoisyPenumbra);
        const auto& hdrColor = ms_Resources->Get(TextureId::HdrColor);

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &ms_D3D12RenderViewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &ms_D3D12RenderScissorRect);
        cmdList.GetD3D12GraphicsCommandList()->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        cmdList.AddTransition(albedoAndRoughness, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(emissiveAndMetallic, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(worldNormal, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(depth, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_DEPTH_READ);
        cmdList.AddTransition(shadow, D3D12_RESOURCE_STATE_GENERIC_READ);
        cmdList.AddTransition(hdrColor, D3D12_RESOURCE_STATE_RENDER_TARGET, true);

        cmdList.SetGraphicsRootResource(*Resources::AlbedoAndRoughness, albedoAndRoughness.GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::EmissiveAndMetallic, emissiveAndMetallic.GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::WorldNormal, worldNormal.GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::DepthStencil, depth.GetSrv());
        cmdList.SetGraphicsRootResource(*Resources::Shadow, shadow.GetSrv());

        cmdList.ClearRenderTarget(hdrColor);
        cmdList.AddRenderTarget(hdrColor);
        cmdList.AddDepthStencil(depth);
        cmdList.SetRenderTargets();

        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::DeferredLighting));
        cmdList.DrawVertexed(3);
    }

}
