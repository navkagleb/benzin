#pragma once

namespace benzin
{

    class Device;
    class SwapChain;
    class Texture;
    class TickTimer;

    class RenderResources
    {
    public:
        using IsResourceFlippableCallback = std::function<bool(uint32_t)>;
        using ForEachTextureCallback = std::function<void(uint32_t index, std::unique_ptr<Texture>&)>;

        explicit RenderResources(IsResourceFlippableCallback&& isResourceFippableCallback);

        std::unique_ptr<Texture>& GetTexture(uint32_t key);
        std::unique_ptr<Texture>& GetPreviousTexture(uint32_t key);

        void ForEachFlippableTexture(uint32_t key, ForEachTextureCallback&& callback);

        void FlipResources();

    public:
        std::unordered_map<uint32_t, std::unique_ptr<Texture>> m_Textures;

        uint32_t m_PreviousFlipResourceIndex = 1;
        uint32_t m_CurrentFlipResourceIndex = 0;

        IsResourceFlippableCallback m_IsResourceFlippableCallback;
    };

    class RenderPass
    {
    public:
        virtual ~RenderPass() = default;

    public:
        static void SetContext(Device& device, SwapChain& swapChain, RenderResources& renderResources);

        auto IsRenderingEnabled() const { return m_IsRenderingEnabled; }
        void SetRenderingEnabled(bool isEnabled) { m_IsRenderingEnabled = isEnabled; }

        virtual void OnZeroFrameInit() {}
        virtual void OnResize(uint32_t width, uint32_t height);

        virtual void OnUpdate() {}
        virtual void OnUpdate(const TickTimer& tickTimer);
        virtual void OnRender() const = 0;

    protected:
        static inline Device* ms_Device = nullptr;
        static inline SwapChain* ms_SwapChain = nullptr;
        static inline RenderResources* ms_RenderResources = nullptr;

        bool m_IsRenderingEnabled = true;
    };

}
