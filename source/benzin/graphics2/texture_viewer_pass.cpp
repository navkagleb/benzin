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
            proxy.m_Cs.m_FileName = "texture_viewer_pass.hlsl";
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

            DXGI_FORMAT dxgiDebugFormat = referenceTexture.GetDxgiFormat();
            if (dxgiDebugFormat == DXGI_FORMAT_D32_FLOAT)
            {
                dxgiDebugFormat = DXGI_FORMAT_R32_FLOAT;
            }

            ms_Resources->Create(
                TextureId::DebugTexture,
                dxgiDebugFormat,
                debugWidth,
                debugHeight,
                TextureAccessFlag::AllowUnorderedAccess);
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
        using Resources = joint::TextureViewerResources;

        BenzinProfile();
        BenzinGpuProfile("TextureViewer");

        ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::TextureViewer));
        cmdList.SetComputeCbv(UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        const Texture& referenceTexture = ms_Resources->Get(m_ReferenceTextureId);
        const Texture& debugTexture = ms_Resources->Get(TextureId::DebugTexture);

        cmdList.SetComputeRootSrv(*Resources::ReferenceTexture, referenceTexture, TextureSrv
        {
            .m_DepthOffset = m_ViewerData.m_ActiveDepthIndex,
            .m_DepthCount = 1,
            .m_MipOffset = m_ViewerData.m_ActiveMipIndex,
            .m_MipCount = 1,
        });

        cmdList.SetComputeRootUav(*Resources::OutDebugTexture, debugTexture);
        cmdList.FlushBarriers();

        cmdList.Dispatch({ debugTexture.GetWidth(), debugTexture.GetHeight(), 1 }, { 16, 16, 1 });

        cmdList.AddUavBarrier(debugTexture);
    }

}
