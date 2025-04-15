#pragma once

#include <benzin/graphics/common.hpp>

namespace benzin
{

    class Buffer;
    class ConstBufferPool;
    class Device;
    class GpuProfiler;
    class PsoManager;
    class RayTracing_Scene;
    class Scene;
    class SwapChain;
    class Texture;
    class TickTimer;

    struct BufferCreation;
    struct TextureCreation;

    class RenderSettings
    {
    public:
        template <typename T>
        T& GetSection()
        {
            const uint64_t hash = typeid(T).hash_code();

            if (!m_Sections.contains(hash))
            {
                m_Sections[hash] = VoidUniquePtr
                {
                    (void*)new T,
                    [](const void* data)
                    {
                        auto ptr = (const T*)data;
                        delete ptr;
                    }
                };
            }

            return *(T*)m_Sections[hash].get();
        }

    private:
        using VoidUniquePtrDeleter = std::function<void(const void*)>;
        using VoidUniquePtr = std::unique_ptr<void, VoidUniquePtrDeleter>;
        std::unordered_map<uint64_t, VoidUniquePtr> m_Sections;
    };

    template <typename ResourceT>
    class RenderResourceStorage
    {
    public:
        using IsResourceFlippableCallback = std::function<bool(uint32_t id)>;
        using IsResourceIdValidCallback = std::function<bool(uint32_t id)>;

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

    extern template class RenderResourceStorage<Buffer>;
    extern template class RenderResourceStorage<Texture>;

    class RenderResources
    {
    public:
        explicit RenderResources(Device& device);
        ~RenderResources();

        // Buffers
        bool IsCreated(BufferId id) const;
        void Create(BufferId id, const BufferCreation& creation);
        void Destroy(BufferId id);

        const Buffer& Get(BufferId id) const;
        const Buffer& GetPrev(BufferId id) const;

        // Textures
        bool IsCreated(TextureId id) const;
        void Create(TextureId id, const TextureCreation& creation);
        void Destroy(TextureId id);

        const Texture& Get(TextureId id) const;
        const Texture& GetPrev(TextureId id) const;

        void FlipResources();

    private:
        static uint8_t GetNextFlipIndex(uint8_t index);

    private:
        Device& m_Device;

        RenderResourceStorage<Buffer> m_Buffers;
        RenderResourceStorage<Texture> m_Textures;

        uint8_t m_FlipIndex = 0;
    };

    class RenderPass
    {
    public:
        RenderPass() = default;
        virtual ~RenderPass() = default;

    public:
        static void SetContext(
            Device& device,
            SwapChain& swapChain,
            GpuProfiler& gpuProfiler,
            PsoManager& psoManager,
            ConstBufferPool& constBufferPool,
            RenderResources& resources,
            RenderSettings& settings,
            const TickTimer& frameTimer,
            const TickTimer& animationTimer,
            const Scene& scene,
            RayTracing_Scene& rayTracingScene
        );

        static void SetWindowViewport(uint32_t width, uint32_t height);
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
        static inline GpuProfiler* ms_GpuProfiler = nullptr;
        static inline PsoManager* ms_PsoManager = nullptr;
        static inline ConstBufferPool* ms_ConstBufferPool = nullptr;
        static inline RenderResources* ms_Resources = nullptr;
        static inline RenderSettings* ms_Settings = nullptr;

        static inline const TickTimer* ms_FrameTimer = nullptr;
        static inline const TickTimer* ms_AnimationTimer = nullptr;

        static inline const Scene* ms_Scene = nullptr;
        static inline RayTracing_Scene* ms_RayTracingScene = nullptr;

        static inline Viewport ms_WindowViewport;
        static inline ScissorRect ms_WindowScissorRect;

        static inline Viewport ms_RenderViewport;
        static inline ScissorRect ms_RenderScissorRect;

        static uint32_t GetWindowViewportWidth() { return (uint32_t)ms_WindowViewport.Width; }
        static uint32_t GetWindowViewportHeight() { return (uint32_t)ms_WindowViewport.Height; }
        static DirectX::XMUINT2 GetWindowResolution() { return { GetWindowViewportWidth(), GetWindowViewportHeight() }; };

        static uint32_t GetRenderViewportWidth() { return (uint32_t)ms_RenderViewport.Width; }
        static uint32_t GetRenderViewportHeight() { return (uint32_t)ms_RenderViewport.Height; }
        static DirectX::XMUINT2 GetRenderResolution() { return { GetRenderViewportWidth(), GetRenderViewportHeight() }; };

        bool m_IsRenderingEnabled = true;
    };

}
