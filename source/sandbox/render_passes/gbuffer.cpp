#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/gbuffer.hpp>

#include <benzin/graphics/cmd_list.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <sandbox/resources.hpp>

namespace sandbox
{

    GBuffer::GBuffer(const benzin::RenderResources& resources)
        : m_AlbedoAndRoughness{ resources.Get(TextureId::AlbedoAndRoughness) }
        , m_EmissiveAndMetallic{ resources.Get(TextureId::EmissiveAndMetallic) }
        , m_WorldNormal{ resources.Get(TextureId::WorldNormal) }
        , m_Mv{ resources.Get(TextureId::Mv) }
        , m_ViewDepth{ resources.Get(TextureId::ViewDepth) }
        , m_DepthStencil{ resources.Get(TextureId::DepthStencil) }
    {}

    void GBuffer::SetRenderTargets(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.SetRenderTargets(
            {
                m_AlbedoAndRoughness.GetRtv(),
                m_EmissiveAndMetallic.GetRtv(),
                m_WorldNormal.GetRtv(),
                m_Mv.GetRtv(),
                m_ViewDepth.GetRtv(),
            },
            &m_DepthStencil.GetDsv());
    }

    void GBuffer::SetDepthStencilOnly(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.SetRenderTargets({}, &m_DepthStencil.GetDsv());
    }

    void GBuffer::ClearRenderTargets(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.ClearRenderTarget(m_AlbedoAndRoughness);
        cmdList.ClearRenderTarget(m_EmissiveAndMetallic);
        cmdList.ClearRenderTarget(m_WorldNormal);
        cmdList.ClearRenderTarget(m_Mv);
        cmdList.ClearRenderTarget(m_ViewDepth);
    }

    void GBuffer::ClearDepthStencil(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.ClearDepthStencil(m_DepthStencil);
    }

    benzin::ScopedResourceBarriers GBuffer::CreateResourceBarriers(
        benzin::GraphicsCmdList& cmdList,
        benzin::ResourceState depthStencilState,
        bool isDepthStencilOnly) const
    {
        std::vector<benzin::ResourceBarrierVariant> resourceBarriers;
        resourceBarriers.reserve(isDepthStencilOnly ? 1 : 6);

        if (!isDepthStencilOnly)
        {
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_AlbedoAndRoughness, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_EmissiveAndMetallic, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_WorldNormal, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_Mv, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_ViewDepth, benzin::ResourceState::RenderTarget });
        }

        BenzinAssert(depthStencilState == benzin::ResourceState::DepthWrite || depthStencilState == benzin::ResourceState::DepthRead);
        resourceBarriers.push_back(benzin::TransitionBarrier{ m_DepthStencil, depthStencilState });

        return benzin::ScopedResourceBarriers
        {
            cmdList,
            resourceBarriers,
        };
    }

}
