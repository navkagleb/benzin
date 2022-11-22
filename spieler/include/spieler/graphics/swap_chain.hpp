#pragma once

#include "spieler/graphics/common.hpp"
#include "spieler/graphics/texture.hpp"

namespace spieler
{

    class Window;

    class Device;
    class CommandQueue;

    enum class VSyncState : bool
    {
        Enabled = true,
        Disabled = false
    };

    class SwapChain
    {
    public:
        SwapChain(const Window& window, Device& device, CommandQueue& commandQueue);
        ~SwapChain();

    public:
        GraphicsFormat GetBufferFormat() const { return m_BufferFormat; }

    public:
        Texture& GetCurrentBuffer();
        const Texture& GetCurrentBuffer() const;

    private:
        uint32_t GetCurrentBufferIndex() const;

    public:
        void ResizeBuffers(Device& device, uint32_t width, uint32_t height);
        void Flip(VSyncState vsync);

    private:
        bool Init(const Window& window, Device& device, CommandQueue& commandQueue);

        bool InitFactory();
        bool InitSwapChain(const Window& window, CommandQueue& commandQueue);

        void EnumerateAdapters();

        void CreateBuffers(Device& device);

    private:
        ComPtr<IDXGIFactory> m_DXGIFactory;
        ComPtr<IDXGISwapChain> m_DXGISwapChain;
        ComPtr<IDXGISwapChain3> m_DXGISwapChain3;
        std::vector<ComPtr<IDXGIAdapter>> m_Adapters;

        GraphicsFormat m_BufferFormat{ GraphicsFormat::R8G8B8A8UnsignedNorm };
        std::vector<Texture> m_Buffers;
    };

} // namespace spieler
