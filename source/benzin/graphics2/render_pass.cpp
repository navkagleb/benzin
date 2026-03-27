#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>

namespace benzin
{

    static constexpr bool IsFlippableTextureExists()
    {
        const auto ids = magic_enum::enum_values<TextureId>();

        for (size_t i = 1; i < ids.size(); ++i)
        {
            if (*ids[i] - *ids[i - 1] == 2)
                return true;
        }

        return false;
    }

    static bool IsTextureFlippable(TextureId id)
    {
        constexpr bool isFlippableResourceExists = IsFlippableTextureExists();

        if (!isFlippableResourceExists)
            return false;

        const uint32_t maxId = *magic_enum::enum_values<TextureId>().back();
        const uint32_t prevId = *id - 1;

        const bool isInBounds = prevId <= maxId;
        const bool isGapExists = !magic_enum::enum_contains<TextureId>(prevId); // There must be a gap between enum values, so the enum value must not exist

        return isInBounds && isGapExists;
    }

    // RenderResources

    RenderResources::RenderResources(GpuHeapLinearAllocator& allocator)
        : m_Allocator{ allocator }
    {
        m_Textures.resize(*magic_enum::enum_values<TextureId>().back() + 1);
    }

    RenderResources::~RenderResources()
    {
#if BENZIN_IS_ASSERTS_ENABLED
        uint32_t aliveCount = 0;
        for (const std::unique_ptr<Texture>& texture : m_Textures)
        {
            aliveCount += texture.get() != nullptr;
        }

        BenzinAssert(aliveCount == 0, "Not all textures are released! Alive texture count: {}", aliveCount);
#endif
    }

    void RenderResources::Create(TextureId id, DXGI_FORMAT dxgiFormat, EnumFlags<TextureAccessFlag> flags)
    {
        Create(
            id,
            dxgiFormat,
            RenderPass::ms_RenderViewportWidth,
            RenderPass::ms_RenderViewportHeight,
            1,
            flags);
    }

    void RenderResources::Create(TextureId id, DXGI_FORMAT dxgiFormat, uint32_t width, uint32_t height, EnumFlags<TextureAccessFlag> flags)
    {
        Create(
            id,
            dxgiFormat,
            width,
            height,
            1,
            flags);
    }

    void RenderResources::Create(TextureId id, DXGI_FORMAT dxgiFormat, uint32_t width, uint32_t height, uint32_t mipCount, EnumFlags<TextureAccessFlag> flags)
    {
        const auto init = [dxgiFormat, width, height, mipCount, flags](TextureCreation& creation)
        {
            creation.m_DxgiFormat = dxgiFormat;
            creation.m_Width = width;
            creation.m_Height = height;
            creation.m_MipCount = mipCount;
            creation.m_AccessFlags = flags;
        };

        if (!IsTextureFlippable(id))
        {
            m_Textures[*id] = m_Allocator.AllocateTexture([id, &init](TextureCreation& creation)
            {
                creation.m_DebugName = magic_enum::enum_name(id);
                init(creation);
            });
        }
        else
        {
            for (uint32_t i = 0; i < 2; ++i)
            {
                m_Textures[*id - i] = m_Allocator.AllocateTexture([id, i, &init](TextureCreation& creation)
                {
                    creation.m_DebugName = std::format("{}{}", magic_enum::enum_name(id), i);
                    init(creation);
                });
            }
        }
    }

    void RenderResources::Destroy(TextureId id)
    {
        if (IsTextureFlippable(id))
        {
            m_Textures[*id - 1].reset();
        }

        m_Textures[*id].reset();
    }

    const Texture* RenderResources::GetPtr(TextureId id) const
    {
        const uint32_t flipIndex = IsTextureFlippable(id) ? m_FlipIndex : 0;
        const std::unique_ptr<Texture>& texture = m_Textures[*id - flipIndex];

        return texture.get();
    }

    const Texture& RenderResources::Get(TextureId id) const
    {
        const uint32_t flipIndex = IsTextureFlippable(id) ? m_FlipIndex : 0;

        const std::unique_ptr<Texture>& texture = m_Textures[*id - flipIndex];
        BenzinAssert(texture.get() != nullptr);

        return *texture;
    }

    const Texture& RenderResources::GetPrev(TextureId id) const
    {
        const uint32_t flipIndex = IsTextureFlippable(id) ? (m_FlipIndex + 1) & 1 : 0;

        const std::unique_ptr<Texture>& texture = m_Textures[*id - flipIndex];
        BenzinAssert(texture.get() != nullptr);

        return *texture;
    }

    void RenderResources::FlipIndex()
    {
        m_FlipIndex = (m_FlipIndex + 1) & 1;
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
        RayTracingScene& rayTracingScene)
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

    void RenderPass::SetWindowSize(uint32_t width, uint32_t height)
    {
        ms_WindowWidth = width;
        ms_WindowHeight = height;
    }

    void RenderPass::SetRenderViewport(uint32_t width, uint32_t height)
    {
        ms_RenderViewportWidth = width;
        ms_RenderViewportHeight = height;

        ms_D3D12RenderViewport.Width = (float)width;
        ms_D3D12RenderViewport.Height = (float)height;
        ms_D3D12RenderViewport.MinDepth = 0.0f;
        ms_D3D12RenderViewport.MaxDepth = 1.0f;

        ms_D3D12RenderScissorRect.right = width;
        ms_D3D12RenderScissorRect.bottom = height;
    }

}
