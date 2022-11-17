#pragma once

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/descriptor_manager.hpp"
#include "spieler/graphics/context.hpp"
#include "spieler/graphics/swap_chain.hpp"

namespace spieler
{

    class Renderer
    {
    private:
        SPIELER_NON_COPYABLE(Renderer)
        SPIELER_NON_MOVEABLE(Renderer)

    public:
        static Renderer& CreateInstance(Window& window);
        static Renderer& GetInstance();
        static void DestroyInstance();

    private:
        Renderer(Window& window);

    public:
        ~Renderer();

    public:
        Device& GetDevice() { return m_Device; }
        Context& GetContext() { return m_Context; }
        SwapChain& GetSwapChain() { return m_SwapChain; }

    public:
        // Swap chain
        void ResizeBuffers(uint32_t width, uint32_t height);
        void Present(VSyncState vsync);

    public:
        Device m_Device;
        Context m_Context;
        SwapChain m_SwapChain;
    };

} // namespace spieler
