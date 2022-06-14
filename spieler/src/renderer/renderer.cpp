#include "spieler/config/bootstrap.hpp"

#include "spieler/renderer/renderer.hpp"

#include "spieler/system/window.hpp"
#include "spieler/renderer/pipeline_state.hpp"

namespace spieler::renderer
{

    static Renderer* g_Instance{ nullptr };

    constexpr SwapChainConfig g_SwapChainConfig
    {
        .BufferCount = 2,
        .BufferFormat = GraphicsFormat::R8G8B8A8_UNORM,
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

    void Renderer::DestoryInstance()
    {
        delete g_Instance;
        g_Instance = nullptr;
    }

    Renderer::Renderer(Window& window)
        : m_Context(m_Device)
        , m_SwapChain(m_Device, m_Context, window, g_SwapChainConfig)
    {}

    Renderer::~Renderer()
    {
        if (m_Device.GetNativeDevice())
        {
            m_Context.FlushCommandQueue();
        }
    }

    bool Renderer::ResizeBuffers(uint32_t width, uint32_t height)
    {
        return m_SwapChain.ResizeBuffers(m_Device, m_Context, width, height);
    }

    bool Renderer::Present(renderer::VSyncState vsync)
    {
        return m_SwapChain.Present(vsync);
    }

} // namespace spieler::renderer