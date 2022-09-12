#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/renderer.hpp"

#include "spieler/core/common.hpp"

#include "spieler/system/window.hpp"

#include "spieler/renderer/pipeline_state.hpp"

namespace spieler::renderer
{

    static Renderer* g_Instance{ nullptr };

    constexpr SwapChainConfig g_SwapChainConfig
    {
        .BufferCount{ 2 },
        .BufferFormat{ GraphicsFormat::R8G8B8A8UnsignedNorm },
    };

    Renderer& Renderer::CreateInstance(Window& window)
    {
        SPIELER_ASSERT(!g_Instance);

        g_Instance = new Renderer(window);

        return *g_Instance;
    }

    Renderer& Renderer::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return *g_Instance;
    }

    void Renderer::DestroyInstance()
    {
        delete g_Instance;
        g_Instance = nullptr;
    }

    Renderer::Renderer(Window& window)
        : m_Context{ m_Device, ConvertMBToBytes(120) }
        , m_SwapChain{ m_Device, m_Context, window, g_SwapChainConfig }
    {}

    Renderer::~Renderer()
    {
        if (m_Device.GetDX12Device())
        {
            m_Context.FlushCommandQueue();
        }
    }

    void Renderer::ResizeBuffers(uint32_t width, uint32_t height)
    {
        m_Context.FlushCommandQueue();
        m_SwapChain.ResizeBuffers(m_Device, width, height);
    }

    void Renderer::Present(renderer::VSyncState vsync)
    {
        m_SwapChain.Present(vsync);
    }

} // namespace spieler::renderer
