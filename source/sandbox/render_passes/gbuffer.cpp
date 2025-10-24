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
        D3D12_RESOURCE_STATES d3d12DepthStencilState,
        bool isDepthStencilOnly) const
    {
        std::vector<benzin::ResourceBarrierVariant> resourceBarriers;
        resourceBarriers.reserve(isDepthStencilOnly ? 1 : 6);

        if (!isDepthStencilOnly)
        {
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_AlbedoAndRoughness, D3D12_RESOURCE_STATE_RENDER_TARGET });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_EmissiveAndMetallic, D3D12_RESOURCE_STATE_RENDER_TARGET });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_WorldNormal, D3D12_RESOURCE_STATE_RENDER_TARGET });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_Mv, D3D12_RESOURCE_STATE_RENDER_TARGET });
            resourceBarriers.push_back(benzin::TransitionBarrier{ m_ViewDepth, D3D12_RESOURCE_STATE_RENDER_TARGET });
        }

        BenzinAssert(d3d12DepthStencilState == D3D12_RESOURCE_STATE_DEPTH_WRITE || d3d12DepthStencilState == D3D12_RESOURCE_STATE_DEPTH_READ);
        resourceBarriers.push_back(benzin::TransitionBarrier{ m_DepthStencil, d3d12DepthStencilState });

        return benzin::ScopedResourceBarriers
        {
            cmdList,
            resourceBarriers,
        };
    }

}
