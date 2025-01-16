#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/render_pass.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    // RenderResources

    RenderResources::RenderResources(Device& device)
        : m_Device{ device }
    {}

    RenderResources::~RenderResources()
    {
#if BENZIN_IS_ASSERTS_ENABLED
        uint32_t nonReleasedTextureCount = 0;
        for (const auto& texture : m_Textures)
        {
            nonReleasedTextureCount += texture.get() != nullptr;
        }

        BenzinAssert(nonReleasedTextureCount == 0, "Not all textures are released! Non released texture count: {}", nonReleasedTextureCount);
#endif
    }

    void RenderResources::SetMaxTextureCount(uint32_t maxTextureCount)
    {
        BenzinAssert(m_Textures.empty());
        m_Textures.resize(maxTextureCount);
    }

    void RenderResources::SetIsTextureFlippableCallback(IsResourceFlippableCallback&& callback)
    {
        m_IsTextureFlippable = std::move(callback);
    }

    void RenderResources::CreateTexture(uint32_t index, const TextureCreation& creation)
    {
        BenzinAssert(index < m_Textures.size());

        if (m_IsTextureFlippable(index))
        {
            auto validatedCreation = creation;

            for (const uint32_t i : std::views::iota(0u, 2u))
            {
                const std::string debugName = std::format("{}{}", creation.DebugName, 0);
                validatedCreation.DebugName = debugName;

                MakeUniquePtr(m_Textures[index - i], m_Device, validatedCreation);
            }

            return;
        }

        MakeUniquePtr(m_Textures[index], m_Device, creation);
    }

    void RenderResources::DestroyTexture(uint32_t index)
    {
        BenzinAssert(index < m_Textures.size());

        if (m_IsTextureFlippable(index))
        {
            m_Textures[index - 1].reset();
        }

        m_Textures[index].reset();
    }

    const Texture& RenderResources::GetTexture(uint32_t index) const
    {
        const auto* texture = GetTexturePtr(index);
        BenzinAssert(texture != nullptr);

        return *texture;
    }

    const Texture& RenderResources::GetPrevTexture(uint32_t index) const
    {
        const auto* texture = GetPrevTexturePtr(index);
        BenzinAssert(texture != nullptr);

        return *texture;
    }

    const Texture* RenderResources::GetTexturePtr(uint32_t index) const
    {
        BenzinAssert(index < m_Textures.size());

        if (m_IsTextureFlippable(index))
        {
            return m_Textures[index - m_FlipIndex].get();
        }

        return m_Textures[index].get();
    }

    const Texture* RenderResources::GetPrevTexturePtr(uint32_t index) const
    {
        BenzinAssert(index < m_Textures.size());
        BenzinAssert(m_IsTextureFlippable(index));

        const uint8_t prevFlipIndex = (m_FlipIndex + 1) & 1;
        return m_Textures[index - prevFlipIndex].get();
    }

    void RenderResources::FlipResources()
    {
        m_FlipIndex = (m_FlipIndex + 1) & 1;
    }

    // RenderPass

    static uint32_t m_RenderPassCount = 0;

    RenderPass::RenderPass()
        : m_GpuTimerIndex{ m_RenderPassCount++ }
    {}

    uint32_t RenderPass::GetRegisteredRenderPassCount()
    {
        return m_RenderPassCount;
    }

    void RenderPass::SetContext(Device& device, SwapChain& swapChain, PsoManager& psoManager, RenderResources& resources, RenderSettings& settings)
    {
        ms_Device = &device;
        ms_SwapChain = &swapChain;
        ms_PsoManager = &psoManager;
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

    void RenderPass::OnUpdate(const TickTimer& tickTimer)
    {
        BenzinUnused(tickTimer);

        OnUpdate();
    };

    ScopedGrabTimer RenderPass::GrabCpuRenderTime()
    {
        return ScopedGrabTimer{ m_CpuRenderTime };
    }

    ScopedGpuGrabTimer RenderPass::GrabGpuRenderTime()
    {
        BenzinAssert(ms_Device != nullptr);
        return ScopedGpuGrabTimer{ ms_Device->GetGpuTimer(), m_GpuTimerIndex };
    }

}
