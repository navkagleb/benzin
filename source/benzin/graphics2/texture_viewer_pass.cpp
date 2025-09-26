#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/texture_viewer_pass.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/game_specific_resource_ids.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::TextureViewerResources);

namespace benzin
{

    TextureViewerPass::TextureViewerPass(const TextureViewerData& viewerData)
        : m_ViewerData{ viewerData }
    {
        ms_PsoManager->Create(PsoId::TextureViewer, [](ComputePsoProxy& proxy)
        {
            proxy.Cs.FileName = "texture_viewer_pass.hlsl";
        });
    }

    TextureViewerPass::~TextureViewerPass()
    {
        ms_PsoManager->Destroy(PsoId::TextureViewer);
        ms_Resources->Destroy(TextureId::DebugTexture);
    }

    void TextureViewerPass::OnUpdate()
    {
        BenzinProfile();

        RenderPass::m_IsRenderingEnabled = m_ViewerData.m_IsRenderingNeeded;
        if (!RenderPass::m_IsRenderingEnabled)
            return;

        const Texture& referenceTexture = ms_Resources->Get(m_ViewerData.m_ReferenceTextureId);

        const uint32_t debugWidth = referenceTexture.GetMipWidth((uint16_t)m_ViewerData.m_ActiveMipIndex);
        const uint32_t debugHeight = referenceTexture.GetMipHeight((uint16_t)m_ViewerData.m_ActiveMipIndex);

        const bool isTextureIdMatch = m_ReferenceTextureId == m_ViewerData.m_ReferenceTextureId;
        const bool isTextureResMatch = m_Consts.TextureResolution.x == debugWidth && m_Consts.TextureResolution.y == debugHeight;

        if (!isTextureIdMatch || !isTextureResMatch)
        {
            m_ReferenceTextureId = m_ViewerData.m_ReferenceTextureId;

            m_Consts.TextureResolution.x = debugWidth;
            m_Consts.TextureResolution.y = debugHeight;

            GraphicsFormat debugFormat = referenceTexture.GetFormat();
            if (debugFormat == GraphicsFormat::D24Unorm_S8Uint)
            {
                debugFormat = GraphicsFormat::R32Float;
            }

            ms_Resources->Create(TextureId::DebugTexture, TextureCreation
            {
                .DebugName = magic_enum::enum_name(TextureId::DebugTexture),
                .Format = debugFormat,
                .Width = debugWidth,
                .Height = debugHeight,
                .Depth = 1,
                .MipCount = 1,
                .AccessFlags = TextureAccessFlag::AllowUnorderedAccess,
            });
        }

        m_Consts.ChannelMask.x = m_ViewerData.m_IsChannelActive[0];
        m_Consts.ChannelMask.y = m_ViewerData.m_IsChannelActive[1];
        m_Consts.ChannelMask.z = m_ViewerData.m_IsChannelActive[2];
        m_Consts.ChannelMask.w = m_ViewerData.m_IsChannelActive[3];

        m_Consts.MinColor = m_ViewerData.m_MinColor;
        m_Consts.MaxColor = m_ViewerData.m_MaxColor;
    }

    void TextureViewerPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("TextureViewer");

        ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const Texture& debugTexture = ms_Resources->Get(TextureId::DebugTexture);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ debugTexture, benzin::ResourceState::UnorderedAccess });

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::TextureViewer));
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        {
            using Resources = joint::TextureViewerResources;

            cmdList.SetComputeRootResource(*Resources::ReferenceTexture, ms_Resources->Get(m_ReferenceTextureId).GetSrv(
            {
                .DepthRange = (uint16_t)m_ViewerData.m_ActiveDepthIndex,
                .MipRange = (uint16_t)m_ViewerData.m_ActiveMipIndex,
            }));

            cmdList.SetComputeRootResource(*Resources::OutDebugTexture, debugTexture.GetUav());
        }

        cmdList.Dispatch({ debugTexture.GetWidth(), debugTexture.GetHeight(), 1 }, { 16, 16, 1 });
    }

}
