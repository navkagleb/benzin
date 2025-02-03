#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/full_screen_debug_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include "sandbox/resources.hpp"
#include "sandbox/sandbox_render_settings.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_FullScreenDebug);

namespace sandbox
{

    FullScreenDebugPass::FullScreenDebugPass()
    {
        ms_PsoManager->CreateGraphicsPso(+Pso::FullScreenDebug, [](benzin::GraphicsPsoProxy& proxy)
        {
            proxy.DebugName = "FullScreenDebugPass";
            proxy.VsFileName = "fullscreen_triangle.hlsl";
            proxy.PsFileName = "fullscreen_debug_pass.hlsl";
            proxy.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle,
            proxy.DepthState = benzin::DepthState
            {
                .IsEnabled = false,
                .IsWriteEnabled = false,
            };
            proxy.RenderTargetFormats.push_back(benzin::GraphicsFormat::Rgba8Unorm);
        });

        ms_ConstBufferPool->PreAllocate<joint::FullScreenDebugConsts>();
    }

    FullScreenDebugPass::~FullScreenDebugPass()
    {
        ms_PsoManager->DestroyPso(+Pso::FullScreenDebug);
    }

    void FullScreenDebugPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<FullScreenDebugSettings>();

        m_IsRenderingEnabled = settings.DebugOutputType != joint::DebugOutputType::None;

        m_Consts.OutputType = settings.DebugOutputType;
        m_Consts.ViewDepthMipIndex = settings.ViewDepthMipIndex;
        m_Consts.MinViewDepth = settings.MinViewDepth;
        m_Consts.MaxViewDepth = settings.MaxViewDepth;
    }

    void FullScreenDebugPass::OnRender() const
    {
        BenzinProfile();

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "FullScreenDebug");

        commandList.SetViewport(ms_RenderViewport);
        commandList.SetScissorRect(ms_RenderScissorRect);

        const auto& finalTexture = ms_Resources->GetTexture(+Texture::Final);

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ finalTexture.GetRtv() });
        commandList.ClearRenderTarget(finalTexture);

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::FullScreenDebug));

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        {
            using enum joint::Rc_FullScreenDebug;

            commandList.SetRootResource(+AlbedoAndRoughness, ms_Resources->GetTexture(+Texture::AlbedoAndRoughness).GetSrv());
            commandList.SetRootResource(+EmissiveAndMetallic, ms_Resources->GetTexture(+Texture::EmissiveAndMetallic).GetSrv());
            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+Mv, ms_Resources->GetTexture(+Texture::Mv).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+Depth, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
            commandList.SetRootResource(+NoisyPenumbra, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());
        }

        commandList.SetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);

        commandList.DrawVertexed(3);
    }

}
