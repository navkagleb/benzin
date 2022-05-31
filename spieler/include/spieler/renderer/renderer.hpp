#pragma once

#include "spieler/renderer/device.hpp"
#include "spieler/renderer/descriptor_manager.hpp"
#include "spieler/renderer/context.hpp"
#include "spieler/renderer/swap_chain.hpp"

namespace spieler::renderer
{

    class Renderer
    {
    private:
        SPIELER_NON_COPYABLE(Renderer)
        SPIELER_NON_MOVEABLE(Renderer)

    public:
        static Renderer& CreateInstance(Window& window);
        static Renderer& GetInstance();
        static void DestoryInstance();

    private:
        Renderer(Window& window);

    public:
        ~Renderer();

    public:
        Device& GetDevice() { return m_Device; }
        Context& GetContext() { return m_Context; }
        SwapChain& GetSwapChain() { return m_SwapChain; }

    public:
        void SetDefaultRenderTargets();

        void ClearRenderTarget(const DirectX::XMFLOAT4& color);
        void ClearDepth(float depth);
        void ClearStencil(uint8_t stencil);
        void ClearDepthStencil(float depth, uint8_t stencil);

        // Swap chain
        bool ResizeBuffers(uint32_t width, uint32_t height);
        bool Present(VSyncState vsync);

    private:
        void ClearDepthStencil(float depth, uint8_t stencil, D3D12_CLEAR_FLAGS flags);

    public:
        Device m_Device;
        Context m_Context;
        SwapChain m_SwapChain;
    };

} // namespace spieler::renderer
