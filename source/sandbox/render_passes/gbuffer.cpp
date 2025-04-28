#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/gbuffer.hpp>

#include <benzin/graphics/cmd_list.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <sandbox/resources.hpp>

namespace sandbox
{

    GBuffer::GBuffer(const benzin::RenderResources& resources)
        : AlbedoAndRoughness{ resources.Get(TextureId::AlbedoAndRoughness) }
        , EmissiveAndMetallic{ resources.Get(TextureId::EmissiveAndMetallic) }
        , WorldNormal{ resources.Get(TextureId::WorldNormal) }
        , Mv{ resources.Get(TextureId::Mv) }
        , ViewDepth{ resources.Get(TextureId::ViewDepth) }
        , DepthStencil{ resources.Get(TextureId::DepthStencil) }
    {}

    void GBuffer::SetRenderTargets(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.SetRenderTargets(
            {
                AlbedoAndRoughness.GetRtv(),
                EmissiveAndMetallic.GetRtv(),
                WorldNormal.GetRtv(),
                Mv.GetRtv(),
                ViewDepth.GetRtv(),
            },
            &DepthStencil.GetDsv()
        );
    }

    void GBuffer::SetDepthStencilOnly(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.SetRenderTargets({}, &DepthStencil.GetDsv());
    }

    void GBuffer::ClearRenderTargets(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.ClearRenderTarget(AlbedoAndRoughness);
        cmdList.ClearRenderTarget(EmissiveAndMetallic);
        cmdList.ClearRenderTarget(WorldNormal);
        cmdList.ClearRenderTarget(Mv);
        cmdList.ClearRenderTarget(ViewDepth);
    }

    void GBuffer::ClearDepthStencil(benzin::GraphicsCmdList& cmdList) const
    {
        cmdList.ClearDepthStencil(DepthStencil);
    }

    benzin::ScopedResourceBarriers GBuffer::CreateResourceBarriers(
        benzin::GraphicsCmdList& cmdList,
        benzin::ResourceState depthStencilState,
        bool isDepthStencilOnly
    ) const
    {
        std::vector<benzin::ResourceBarrierVariant> resourceBarriers;
        resourceBarriers.reserve(isDepthStencilOnly ? 1 : 6);

        if (!isDepthStencilOnly)
        {
            resourceBarriers.push_back(benzin::TransitionBarrier{ AlbedoAndRoughness, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ EmissiveAndMetallic, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ WorldNormal, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ Mv, benzin::ResourceState::RenderTarget });
            resourceBarriers.push_back(benzin::TransitionBarrier{ ViewDepth, benzin::ResourceState::RenderTarget });
        }

        BenzinAssert(depthStencilState == benzin::ResourceState::DepthWrite || depthStencilState == benzin::ResourceState::DepthRead);
        resourceBarriers.push_back(benzin::TransitionBarrier{ DepthStencil, depthStencilState });

        return benzin::ScopedResourceBarriers
        {
            cmdList,
            resourceBarriers,
        };
    }

}
