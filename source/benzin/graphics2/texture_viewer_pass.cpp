#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/texture_viewer_pass.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/game_specific_resource_ids.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/tools/texture_viewer_tool.hpp>

BenzinEnableUnaryPlusForEnum(joint::TextureViewerResources)

namespace benzin
{

    TextureViewerPass::TextureViewerPass(const TextureViewerTool& textureViewerTool)
        : m_TextureViewerTool{ textureViewerTool }
    {
        ms_PsoManager->Create(PsoId::TextureViewer, [](ComputePsoProxy& proxy)
        {
            proxy.Cs.FileName = "texture_viewer_pass.hlsl";
        });

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
    }

    TextureViewerPass::~TextureViewerPass()
    {
        ms_PsoManager->Destroy(PsoId::TextureViewer);
        ms_Resources->Destroy(TextureId::DebugTexture);
    }

    void TextureViewerPass::OnRenderViewportResize()
    {
        if (m_ReferenceTextureId != g_InvalidTextureId)
        {
            CreateDebugTexture();
        }
    }

    void TextureViewerPass::OnUpdate()
    {
        BenzinProfile();

        RenderPass::m_IsRenderingEnabled = m_TextureViewerTool.IsReferenceTextureIdValid();
        RenderPass::m_IsRenderingEnabled &= m_TextureViewerTool.m_IsVisible;
        RenderPass::m_IsRenderingEnabled &= m_TextureViewerTool.m_IsCollapsed ? m_TextureViewerTool.m_IsFullViewportPreview : true;

        if (!RenderPass::m_IsRenderingEnabled)
        {
            return;
        }

        if (m_ReferenceTextureId != m_TextureViewerTool.m_ReferenceTextureId)
        {
            m_ReferenceTextureId = m_TextureViewerTool.m_ReferenceTextureId;

            CreateDebugTexture();
        }

        m_Consts.ChannelMask.x = m_TextureViewerTool.m_IsChannelActive[0];
        m_Consts.ChannelMask.y = m_TextureViewerTool.m_IsChannelActive[1];
        m_Consts.ChannelMask.z = m_TextureViewerTool.m_IsChannelActive[2];
        m_Consts.ChannelMask.w = m_TextureViewerTool.m_IsChannelActive[3];

        m_Consts.MinColor = m_TextureViewerTool.m_MinColor;
        m_Consts.MaxColor = m_TextureViewerTool.m_MaxColor;
    }

    void TextureViewerPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "TextureViewer");

        const auto& debugTexture = ms_Resources->Get(TextureId::DebugTexture);

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ debugTexture, benzin::ResourceState::UnorderedAccess },
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::TextureViewer));
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        {
            using enum joint::TextureViewerResources;

            cmdList.SetComputeRootResource(+ReferenceTexture, ms_Resources->Get(m_ReferenceTextureId).GetSrv({ .DepthRange{ m_TextureViewerTool.m_ActiveDepthIndex } }));
            cmdList.SetComputeRootResource(+OutDebugTexture, debugTexture.GetUav());
        }

        cmdList.Dispatch({ debugTexture.GetWidth(), debugTexture.GetHeight(), 1 }, { 16, 16, 1 });
    }

    void TextureViewerPass::CreateDebugTexture()
    {
        const Texture& referenceTexture = ms_Resources->Get(m_ReferenceTextureId);
        ms_Resources->Create(TextureId::DebugTexture, TextureCreation
        {
            .DebugName = "DebugTexture",
            .Format = referenceTexture.GetFormat(),
            .Width = referenceTexture.GetWidth(),
            .Height = referenceTexture.GetHeight(),
            .Depth = 1,
            .MipCount = 1, // TODO: Add support multiple mip levels
            .AccessFlags = TextureAccessFlag::AllowUnorderedAccess,
        });

        m_Consts.TextureResolution.x = referenceTexture.GetWidth();
        m_Consts.TextureResolution.y = referenceTexture.GetHeight();
    }

}
