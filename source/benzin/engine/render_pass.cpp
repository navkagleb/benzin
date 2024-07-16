#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/render_pass.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    // RenderResources

    void RenderResources::SetIsTextureFlippableResources(IsResourceFlippableCallback&& callback)
    {
        m_IsTextureFlippableCallback = std::move(callback);
    }

    std::unique_ptr<Texture>& RenderResources::GetTexture(uint32_t key)
    {
        if (!m_IsTextureFlippableCallback(key))
        {
            return m_Textures[key];
        }

        return m_Textures[key - m_CurrentFlipResourceIndex];
    }

    std::unique_ptr<Texture>& RenderResources::GetPreviousTexture(uint32_t key)
    {
        BenzinEnsure(m_IsTextureFlippableCallback(key));
        return m_Textures[key - m_PreviousFlipResourceIndex];
    }

    void RenderResources::ForEachFlippableTexture(uint32_t key, ForEachTextureCallback&& callback)
    {
        BenzinEnsure(m_IsTextureFlippableCallback(key));
        
        callback(0, GetTexture(key));
        callback(1, GetPreviousTexture(key));
    }

    void RenderResources::FlipResources()
    {
        m_PreviousFlipResourceIndex = m_CurrentFlipResourceIndex;
        m_CurrentFlipResourceIndex = (m_CurrentFlipResourceIndex + 1) % 2;
    }

    // RenderPass

    void RenderPass::SetContext(Device& device, SwapChain& swapChain, RenderResources& resources, RenderSettings& settings)
    {
        ms_Device = &device;
        ms_SwapChain = &swapChain;
        ms_Resources = &resources;
        ms_Settings = &settings;
    }

    void RenderPass::SetWindowViewport(uint32_t width, uint32_t height)
    {
        ms_WindowViewport.Width = (float)width;
        ms_WindowViewport.Height = (float)height;

        ms_WindowScissorRect.Width = (float)width;
        ms_WindowScissorRect.Height = (float)height;
    }

    void RenderPass::SetRenderViewport(uint32_t width, uint32_t height)
    {
        ms_RenderViewport.Width = (float)width;
        ms_RenderViewport.Height = (float)height;

        ms_RenderScissorRect.Width = (float)width;
        ms_RenderScissorRect.Height = (float)height;
    }

    void RenderPass::OnWindowResize(uint32_t width, uint32_t height)
    {
        BenzinUnused(width);
        BenzinUnused(height);
    };

    void RenderPass::OnRenderViewportResize(uint32_t width, uint32_t height)
    {
        BenzinUnused(width);
        BenzinUnused(height);
    }

    void RenderPass::OnUpdate(const TickTimer& tickTimer)
    {
        BenzinUnused(tickTimer);

        OnUpdate();
    };

}
