#pragma once

#include "benzin/graphics/common.hpp"

namespace benzin
{

    class ConstBufferPool;
    class Device;
    class PsoManager;
    class ScopedGpuGrabTimer;
    class SwapChain;
    class Texture;
    class TickTimer;

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

    class RenderResources
    {
    public:
        using IsResourceFlippableCallback = std::function<bool(uint32_t)>;

        explicit RenderResources(Device& device);
        ~RenderResources();

        void SetMaxTextureCount(uint32_t maxTextureCount);
        void SetIsTextureFlippableCallback(IsResourceFlippableCallback&& callback);

        void CreateTexture(uint32_t index, const TextureCreation& creation);
        void DestroyTexture(uint32_t index);

        const Texture& GetTexture(uint32_t index) const;
        const Texture& GetPrevTexture(uint32_t index) const;

        const Texture* GetTexturePtr(uint32_t index) const;
        const Texture* GetPrevTexturePtr(uint32_t index) const;

        void FlipResources();

    public:
        Device& m_Device;

        uint8_t m_FlipIndex = 0;

        std::vector<std::unique_ptr<Texture>> m_Textures;

        IsResourceFlippableCallback m_IsTextureFlippable;
    };

    class RenderPass
    {
    public:
        RenderPass();
        virtual ~RenderPass() = default;

    public:
        static uint32_t GetRegisteredRenderPassCount();

        static void SetContext(
            Device& device,
            SwapChain& swapChain,
            PsoManager& psoManager,
            ConstBufferPool& constBufferPool,
            RenderResources& resources,
            RenderSettings& settings
        );

        static void SetWindowViewport(uint32_t width, uint32_t height);
        static void SetRenderViewport(uint32_t width, uint32_t height);

        auto GetGpuTimerIndex() const { return m_GpuTimerIndex; }
        auto IsRenderingEnabled() const { return m_IsRenderingEnabled; }

        auto GetCpuRenderTime() const { return m_CpuRenderTime; }

        virtual bool IsDependentOnViewport() const = 0;

        virtual void OnZeroFrameInit() {}
        virtual void OnWindowResize() {}
        virtual void OnRenderViewportResize() {}

        virtual void OnUpdate() {}
        virtual void OnUpdate(const TickTimer& tickTimer);
        virtual void OnRender() const = 0;

        [[nodiscard]] ScopedGrabTimer GrabCpuRenderTime();
        [[nodiscard]] ScopedGpuGrabTimer GrabGpuRenderTime();

    protected:
        static inline Device* ms_Device = nullptr;
        static inline SwapChain* ms_SwapChain = nullptr;
        static inline PsoManager* ms_PsoManager = nullptr;
        static inline ConstBufferPool* ms_ConstBufferPool = nullptr;
        static inline RenderResources* ms_Resources = nullptr;
        static inline RenderSettings* ms_Settings = nullptr;

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

        uint32_t m_GpuTimerIndex = g_InvalidUnsigned<uint32_t>;
        bool m_IsRenderingEnabled = true;

        std::chrono::microseconds m_CpuRenderTime = std::chrono::microseconds::zero();
    };

}
