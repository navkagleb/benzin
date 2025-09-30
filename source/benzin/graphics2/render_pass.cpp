#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics2/game_specific_resource_ids.hpp>

namespace benzin
{

    template <typename ResourceIdT> requires std::is_enum_v<ResourceIdT>
    static uint32_t GetMaxResourceCount()
    {
        const auto maxResourceId = +magic_enum::enum_values<ResourceIdT>().back();

        return maxResourceId + 1;
    }

    template <typename ResourceIdT> requires std::is_enum_v<ResourceIdT>
    static bool IsFlippableResourceExists()
    {
        const auto ids = magic_enum::enum_values<ResourceIdT>();

        for (size_t i = 1; i < ids.size(); ++i)
        {
            if (+ids[i] - +ids[i - 1] == 2)
            {
                return true;
            }
        }

        return false;
    }

    template <typename ResourceIdT> requires std::is_enum_v<ResourceIdT>
    static bool IsResourceFlippable(uint32_t id)
    {
        static const bool isFlippableResourceExists = IsFlippableResourceExists<ResourceIdT>();

        if (!isFlippableResourceExists)
        {
            return false;
        }

        const uint32_t maxId = +magic_enum::enum_values<ResourceIdT>().back();
        const uint32_t prevId = id - 1;

        const bool isInBounds = prevId <= maxId;
        const bool isGapExists = !magic_enum::enum_contains<ResourceIdT>(prevId); // There must be a gap between enum values, so the enum value must not exist

        return isInBounds && isGapExists;
    }

    template <typename ResourceIdT> requires std::is_enum_v<ResourceIdT>
    static bool IsResourceIdValid(uint32_t id)
    {
        return magic_enum::enum_contains<ResourceIdT>(id);
    }

    template <typename ResourceT>
    RenderResourceStorage<ResourceT>::RenderResourceStorage(
        uint32_t maxResourceCount,
        IsResourceFlippableCallback&& isRsourceFlippableCallback,
        IsResourceIdValidCallback&& isResourceIdValidCallback
    )
        : m_IsResourceFlippableCallback{ std::move(isRsourceFlippableCallback) }
        , m_IsResourceIdValidCallback{ std::move(isResourceIdValidCallback) }
    {
        BenzinEnsure(m_IsResourceFlippableCallback);
        BenzinAssert(m_IsResourceIdValidCallback);

        m_Resources.resize(maxResourceCount);
    }

    template <typename ResourceT>
    RenderResourceStorage<ResourceT>::~RenderResourceStorage() = default;

    template <typename ResourceT>
    bool RenderResourceStorage<ResourceT>::IsCreated(uint32_t id) const
    {
        BenzinAssert(m_IsResourceIdValidCallback(id));

        return m_Resources[+id].get() != nullptr;
    }

    template <typename ResourceT>
    template <typename CreationT>
    void RenderResourceStorage<ResourceT>::Create(uint32_t id, Device& device, const CreationT& creation)
    {
        static_assert(
            (std::is_same_v<Buffer, ResourceT> && std::is_same_v<BufferCreation, CreationT>) ||
            (std::is_same_v<Texture, ResourceT> && std::is_same_v<TextureCreation, CreationT>)
        );

        BenzinAssert(m_IsResourceIdValidCallback(id));
        BenzinAssert(!creation.DebugName.empty());

        if (m_IsResourceFlippableCallback(id))
        {
            const std::string_view referenceDebugName = creation.DebugName;
            auto& nonConstCreation = const_cast<CreationT&>(creation);

            for (uint32_t i = 0; i < 2; ++i)
            {
                const std::string debugName = std::format("{}{}", referenceDebugName, i);
                nonConstCreation.DebugName = debugName;

                MakeUniquePtr(m_Resources[id - i], device, nonConstCreation);
            }

            return;
        }

        MakeUniquePtr(m_Resources[id], device, creation);
    }

    template <typename ResourceT>
    void RenderResourceStorage<ResourceT>::Destroy(uint32_t id)
    {
        BenzinAssert(m_IsResourceIdValidCallback(id));

        if (m_IsResourceFlippableCallback(id))
        {
            m_Resources[id - 1].reset();
        }

        m_Resources[id].reset();
    }

    template <typename ResourceT>
    const ResourceT& RenderResourceStorage<ResourceT>::Get(uint32_t id, uint8_t flipOffset) const
    {
        BenzinAssert(m_IsResourceIdValidCallback(id));

        if (!m_IsResourceFlippableCallback(id))
        {
            flipOffset = 0;
        }

        BenzinAssert(flipOffset == 0 || flipOffset == 1);

        const auto& resource = m_Resources[id - flipOffset];
        BenzinAssert(resource.get() != nullptr);

        return *resource;
    }

    template <typename ResourceT>
    const ResourceT& RenderResourceStorage<ResourceT>::GetPrev(uint32_t id, uint8_t flipOffset) const
    {
        BenzinAssert(m_IsResourceIdValidCallback(id));
        BenzinAssert(m_IsResourceFlippableCallback(id));
        BenzinAssert(flipOffset == 0 || flipOffset == 1);

        const auto& resource = m_Resources[id - flipOffset];
        BenzinAssert(resource.get() != nullptr);

        return *resource;
    }

#if BENZIN_IS_ASSERTS_ENABLED
    template <typename ResourceT>
    uint32_t RenderResourceStorage<ResourceT>::GetAliveResourceCount() const
    {
        uint32_t aliveCount = 0;
        for (const auto& resource : m_Resources)
        {
            aliveCount += resource.get() != nullptr;
        }

        return aliveCount;
    }
#endif

    template class RenderResourceStorage<Buffer>;
    template class RenderResourceStorage<Texture>;

    // RenderResources

    RenderResources::RenderResources(Device& device)
        : m_Device{ device }
        , m_Buffers{ GetMaxResourceCount<BufferId>(), IsResourceFlippable<BufferId>, IsResourceIdValid<BufferId> }
        , m_Textures{ GetMaxResourceCount<TextureId>(), IsResourceFlippable<TextureId>, IsResourceIdValid<TextureId> }
    {}

    RenderResources::~RenderResources()
    {
#if BENZIN_IS_ASSERTS_ENABLED
        const uint32_t aliveBufferCount = m_Buffers.GetAliveResourceCount();
        const uint32_t aliveTextureCount = m_Buffers.GetAliveResourceCount();

        BenzinAssert(aliveBufferCount == 0, "Not all buffers are released! Alive buffer count: {}", aliveBufferCount);
        BenzinAssert(aliveTextureCount == 0, "Not all textures are released! Alive texture count: {}", aliveTextureCount);
#endif
    }

    bool RenderResources::IsCreated(BufferId id) const
    {
        return m_Buffers.IsCreated(+id);
    }

    void RenderResources::Create(BufferId id, const BufferCreation& creation)
    {
        m_Buffers.Create(+id, m_Device, creation);
    }

    void RenderResources::Destroy(BufferId id)
    {
        m_Buffers.Destroy(+id);
    }

    const Buffer& RenderResources::Get(BufferId id) const
    {
        return m_Buffers.Get(+id, m_FlipIndex);
    }

    const Buffer& RenderResources::GetPrev(BufferId id) const
    {
        return m_Buffers.GetPrev(+id, m_FlipIndex);
    }

    bool RenderResources::IsCreated(TextureId id) const
    {
        return m_Textures.IsCreated(+id);
    }

    void RenderResources::Create(TextureId id, const TextureCreation& creation)
    {
        m_Textures.Create(+id, m_Device, creation);
    }
    
    void RenderResources::Destroy(TextureId id)
    {
        m_Textures.Destroy(+id);
    }

    const Texture& RenderResources::Get(TextureId id) const
    {
        return m_Textures.Get(+id, m_FlipIndex);
    }

    const Texture& RenderResources::GetPrev(TextureId id) const
    {
        return m_Textures.GetPrev(+id, GetNextFlipIndex(m_FlipIndex));
    }

    void RenderResources::FlipResources()
    {
        m_FlipIndex = GetNextFlipIndex(m_FlipIndex);
    }

    uint8_t RenderResources::GetNextFlipIndex(uint8_t index)
    {
        return (index + 1) & 1;
    }

    // RenderViewport

    void RenderViewport::DeferResize(uint32_t width, uint32_t height)
    {
        BenzinAssert(m_Size.x != width || m_Size.y != height);

        m_PendingSize.x = width;
        m_PendingSize.y = height;
    }

    void RenderViewport::Resize()
    {
        BenzinAssert(m_PendingSize.x != 0 && m_PendingSize.y != 0);

        m_Size = std::exchange(m_PendingSize, {});
    }

    // RenderPass

    void RenderPass::SetContext(
        Device& device,
        SwapChain& swapChain,
        PsoManager& psoManager,
        RenderResources& resources,
        RenderSettings& settings,
        const TickTimer& frameTimer,
        const TickTimer& animationTimer,
        const Scene& scene,
        RayTracing_Scene& rayTracingScene
    )
    {
        ms_Device = &device;
        ms_SwapChain = &swapChain;
        ms_PsoManager = &psoManager;
        ms_Resources = &resources;
        ms_Settings = &settings;

        ms_FrameTimer = &frameTimer;
        ms_AnimationTimer = &animationTimer;

        ms_Scene = &scene;
        ms_RayTracingScene = &rayTracingScene;
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

}
