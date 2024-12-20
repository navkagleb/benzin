#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/full_screen_debug_pass.hpp"

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/gpu_timer.hpp>

#include <shaders/joint/root_constants.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    FullScreenDebugPass::FullScreenDebugPass()
    {
        m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::GraphicsPipelineStateCreation
        {
            .DebugName = "FullScreenDebugPass",
            .VsFileName = "fullscreen_triangle.hlsl",
            .PsFileName = "fullscreen_debug_pass.hlsl",
            .PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            .DepthState
            {
                .IsEnabled = false,
                .IsWriteEnabled = false,
            },
            .RenderTargetFormats{ benzin::GraphicsFormat::Rgba8Unorm },
        });

        benzin::MakeUniquePtr(m_PassConstantBuffer, *ms_Device, "FullScreenDebugConstantBuffer");
    }

    FullScreenDebugPass::~FullScreenDebugPass()
    {
        ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);
    }

    void FullScreenDebugPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<FullScreenDebugSettings>();

        m_IsRenderingEnabled = settings.DebugOutputType != joint::DebugOutputType_None;

        m_PassConstantBuffer->UpdateConstants(joint::FullScreenDebugConstants
        {
            .OutputType = magic_enum::enum_integer(settings.DebugOutputType),
            .ViewDepthMipIndex = settings.ViewDepthMipIndex,
            .MinViewDepth = settings.MinViewDepth,
            .MaxViewDepth = settings.MaxViewDepth,
        });
    }

    void FullScreenDebugPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "FullScreenDebugPass");

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ finalTexture.GetRtv() });
        commandList.ClearRenderTarget(finalTexture);

        commandList.SetPipelineState(*m_Pso);

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstantBuffer->GetActiveGpuVirtualAddress());
        commandList.SetRootResource(joint::FullScreenDebugRc_AlbedoAndRoughnessTexture, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_EmissiveAndMetallicTexture, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_WorldNormalTexture, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_VelocityBuffer, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_ViewDepthBuffer, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_DepthBuffer, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
        commandList.SetRootResource(joint::FullScreenDebugRc_NoisyPenumbraTexture, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);

        commandList.DrawVertexed(3);
    }

}
