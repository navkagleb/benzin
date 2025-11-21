#pragma once

#include <benzin/graphics2/game_specific_resource_ids.hpp>

namespace benzin
{

    class Buffer;
    class Device;
    class PsoManager;
    class RayTracingScene;
    class SwapChain;
    class Texture;
    class TickTimer;
    struct BufferCreation;
    struct Scene;
    struct TextureCreation;

    class RenderSettings
    {
    public:
        template <typename SectionT>
        SectionT& GetSection()
        {
            const void* key = GetSectionKey<SectionT>();

            if (!m_Sections.contains(key))
            {
                m_Sections[key] = VoidUniquePtr
                {
                    (void*)new SectionT,
                    [](const void* data)
                    {
                        auto ptr = (const SectionT*)data;
                        delete ptr;
                    }
                };
            }

            return *(SectionT*)m_Sections[key].get();
        }

    private:
        using VoidUniquePtrDeleter = std::function<void(const void*)>;
        using VoidUniquePtr = std::unique_ptr<void, VoidUniquePtrDeleter>;

        template <typename SectionT>
        const void* GetSectionKey()
        {
            static int s_UniqueKey;
            return &s_UniqueKey;
        }

        std::unordered_map<const void*, VoidUniquePtr> m_Sections;
    };

    template <typename ResourceT>
    class RenderResourceStorage
    {
    public:
        using IsResourceFlippableCallback = std::function<bool(uint32_t id)>;
        using IsResourceIdValidCallback = std::function<bool(uint32_t id)>;

        template <typename CreationT>
        using ResourceConfigurator = std::function<void(CreationT& outCreation)>; // TODO

        RenderResourceStorage(
            uint32_t maxResourceCount,
            IsResourceFlippableCallback&& isRsourceFlippableCallback,
            IsResourceIdValidCallback&& isResourceIdValidCallback
        );
        ~RenderResourceStorage();

        bool IsCreated(uint32_t id) const;

        template <typename CreationT>
        void Create(uint32_t id, Device& device, const CreationT& creation);

        void Destroy(uint32_t id);

        const ResourceT& Get(uint32_t id, uint8_t flipOffset) const;
        const ResourceT& GetPrev(uint32_t id, uint8_t flipOffset) const;

#if BENZIN_IS_ASSERTS_ENABLED
        uint32_t GetAliveResourceCount() const;
#endif

    private:
        const IsResourceFlippableCallback m_IsResourceFlippableCallback;
        const IsResourceIdValidCallback m_IsResourceIdValidCallback;

        std::vector<std::unique_ptr<ResourceT>> m_Resources;
    };

    extern template class RenderResourceStorage<Texture>;

    class RenderResources
    {
    public:
        explicit RenderResources(Device& device);
        ~RenderResources();

        bool IsCreated(TextureId id) const;
        void Create(TextureId id, const TextureCreation& creation);
        void Destroy(TextureId id);

        const Texture& Get(TextureId id) const;
        const Texture& GetPrev(TextureId id) const;

        void FlipResources();

    private:
        static uint8_t GetNextFlipIndex(uint8_t index);

        Device& m_Device;

        RenderResourceStorage<Texture> m_Textures;
        uint8_t m_FlipIndex = 0;
    };

    class RenderViewport
    {
    public:
        friend class RenderViewportTool;
        friend class TextureViewerTool;

        auto GetWidth() const { return m_Size.x; }
        auto GetHeight() const { return m_Size.y; }

        auto GetCursorPosition() const { return m_CursorPosition; }

        auto IsPendingResize() const { return m_PendingSize.x != 0 && m_PendingSize.y != 0; }
        auto IsHovered() const { return m_IsHovered; }
        auto IsValidForRendering() const { return m_IsValidForRendering; }

        void DeferResize(uint32_t width, uint32_t height);
        void Resize();

    private:
        TextureId m_DisplayTextureId = TextureId::Final;

        DirectX::XMUINT2 m_Size = {};
        DirectX::XMUINT2 m_PendingSize = {};
        DirectX::XMINT2 m_CursorPosition = { -1, -1 };

        bool m_IsHovered = false;
        bool m_IsValidForRendering = false;
    };

    class RenderPass
    {
    public:
        RenderPass() = default;
        virtual ~RenderPass() = default;

        static void SetContext(
            Device& device,
            SwapChain& swapChain,
            PsoManager& psoManager,
            RenderResources& resources,
            RenderSettings& settings,
            const TickTimer& frameTimer,
            const TickTimer& animationTimer,
            const Scene& scene,
            RayTracingScene& rayTracingScene);

        static void SetWindowSize(uint32_t width, uint32_t height);
        static void SetRenderViewport(uint32_t width, uint32_t height);

        auto IsRenderingEnabled() const { return m_IsRenderingEnabled; }

        virtual bool IsDependentOnViewport() const = 0;

        virtual void OnZeroFrameInit() {}
        virtual void OnWindowResize() {}
        virtual void OnRenderViewportResize() {}

        virtual void OnUpdate() {};
        virtual void OnRender() const = 0;

    protected:
        static inline Device* ms_Device = nullptr;
        static inline SwapChain* ms_SwapChain = nullptr;
        static inline PsoManager* ms_PsoManager = nullptr;
        static inline RenderResources* ms_Resources = nullptr;
        static inline RenderSettings* ms_Settings = nullptr;

        static inline const TickTimer* ms_FrameTimer = nullptr;
        static inline const TickTimer* ms_AnimationTimer = nullptr;

        static inline const Scene* ms_Scene = nullptr;
        static inline RayTracingScene* ms_RayTracingScene = nullptr;

        static inline uint32_t ms_WindowWidth = 0;
        static inline uint32_t ms_WindowHeight = 0;
        static inline uint32_t ms_RenderViewportWidth = 0;
        static inline uint32_t ms_RenderViewportHeight = 0;

        static inline D3D12_VIEWPORT ms_D3D12RenderViewport = {};
        static inline D3D12_RECT ms_D3D12RenderScissorRect = {};

        bool m_IsRenderingEnabled = true;
    };

}
