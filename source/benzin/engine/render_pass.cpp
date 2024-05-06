#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/render_pass.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    // RenderResources

    RenderResources::RenderResources(IsResourceFlippableCallback&& isResourceFippableCallback)
        : m_IsResourceFlippableCallback{ std::move(isResourceFippableCallback) }
    {
        BenzinEnsure((bool)m_IsResourceFlippableCallback);
    }

    std::unique_ptr<Texture>& RenderResources::GetTexture(uint32_t key)
    {
        if (!m_IsResourceFlippableCallback(key))
        {
            return m_Textures[key];
        }

        return m_Textures[key - m_CurrentFlipResourceIndex];
    }

    std::unique_ptr<Texture>& RenderResources::GetPreviousTexture(uint32_t key)
    {
        BenzinEnsure(m_IsResourceFlippableCallback(key));
        return m_Textures[key - m_PreviousFlipResourceIndex];
    }

    void RenderResources::ForEachFlippableTexture(uint32_t key, ForEachTextureCallback&& callback)
    {
        BenzinEnsure(m_IsResourceFlippableCallback(key));
        
        callback(0, GetTexture(key));
        callback(1, GetPreviousTexture(key));
    }

    void RenderResources::FlipResources()
    {
        m_PreviousFlipResourceIndex = m_CurrentFlipResourceIndex;
        m_CurrentFlipResourceIndex = (m_CurrentFlipResourceIndex + 1) % 2;
    }

    // RenderPass

    void RenderPass::SetContext(Device& device, SwapChain& swapChain, RenderResources& renderResources)
    {
        ms_Device = &device;
        ms_SwapChain = &swapChain;
        ms_RenderResources = &renderResources;
    }

    void RenderPass::OnResize(uint32_t width, uint32_t height)
    {
        BenzinUnused(width);
        BenzinUnused(height);
    };

    void RenderPass::OnUpdate(const TickTimer& tickTimer)
    {
        BenzinUnused(tickTimer);

        OnUpdate();
    };

}
