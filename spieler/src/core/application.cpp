#include "spieler/config/bootstrap.hpp"

#include "spieler/core/application.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/imgui/imgui_impl_dx12.h>
#include <third_party/imgui/imgui_impl_win32.h>

#include <third_party/fmt/format.h>

#include "platform/win64_window.hpp"
#include "platform/win64_input.hpp"

#include "spieler/core/common.hpp"

#include "spieler/system/event_dispatcher.hpp"

namespace spieler
{

    Application* Application::GetInstance()
    {
        SPIELER_ASSERT(g_Instance);

        return g_Instance;
    }

    Application::Application()
    {
        SPIELER_ASSERT(!g_Instance);
    }

    Application::~Application()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        renderer::Renderer::DestoryInstance();
    }

    bool Application::InitInternal(const std::string& title, uint32_t width, uint32_t height)
    {
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        // TODO: Init "Subsystems"
        SPIELER_RETURN_IF_FAILED(InitWindow(title, width, height));
        renderer::Renderer::CreateInstance(*g_Instance->m_Window);
        SPIELER_RETURN_IF_FAILED(InitImGui());
        SPIELER_RETURN_IF_FAILED(InitExternal());

        UpdateScreenViewport();
        UpdateScreenScissorRect();

        return true;
    }

    void Application::Run()
    {
        m_Timer.Reset();

        m_ApplicationProps.IsRunning = true;

        while (m_ApplicationProps.IsRunning)
        {
            m_Window->ProcessEvents();
            
            if (m_ApplicationProps.IsPaused)
            {
                ::Sleep(100);
            }
            else
            {
                m_Timer.Tick();

                const float dt{ m_Timer.GetDeltaTime() };

                CalcStats(dt);

                ImGui_ImplDX12_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();

                OnUpdate(dt);
                OnImGuiRender(dt);
                ImGui::Render();

                if (!OnRender(dt))
                {
                    break;
                }
            }
        }

        renderer::Renderer::DestoryInstance();
    }

    bool Application::InitWindow(const std::string& title, uint32_t width, uint32_t height)
    {
#if defined(SPIELER_WIN64)
        m_Window = std::make_unique<Win64_Window>(title, width, height);
#else
        static_assert(false);
#endif

        m_Window->SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });

        return true;
    }

    bool Application::InitImGui()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io{ ImGui::GetIO() };
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.IniFilename = "config/imgui.ini";

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer::Device& device{ renderer.GetDevice() };
        renderer::SwapChain& swapChain{ renderer.GetSwapChain() };
        renderer::DescriptorManager& descriptorManager{ device.GetDescriptorManager() };

        renderer::SRVDescriptor imguiDescriptor{ descriptorManager.AllocateSRV() };

        SPIELER_RETURN_IF_FAILED(ImGui_ImplWin32_Init(m_Window->GetNativeHandle()));
        SPIELER_RETURN_IF_FAILED(ImGui_ImplDX12_Init(
            device.GetNativeDevice().Get(),
            1,
            static_cast<DXGI_FORMAT>(swapChain.GetBufferFormat()),
            descriptorManager.GetDescriptorHeap(renderer::DescriptorHeapType::SRV).GetNative().Get(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ imguiDescriptor.CPU },
            D3D12_GPU_DESCRIPTOR_HANDLE{ imguiDescriptor.GPU }
        ));

        return true;
    }

    void Application::OnUpdate(float dt)
    {
        m_LayerStack.OnUpdate(dt);
    }

    bool Application::OnRender(float dt)
    {
        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer::Device& device{ renderer.GetDevice() };
        renderer::Context& context{ renderer.GetContext() };
        renderer::SwapChain& swapChain{ renderer.GetSwapChain() };
        renderer::DescriptorManager& descriptorManager{ device.GetDescriptorManager() };

        SPIELER_RETURN_IF_FAILED(m_LayerStack.OnRender(dt));

        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {
            context.SetViewport(m_ScreenViewport);
            context.SetScissorRect(m_ScreenScissorRect);

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &swapChain.GetCurrentBuffer().GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::RenderTarget
            });

            context.SetRenderTarget(swapChain.GetCurrentBuffer().GetView<spieler::renderer::RenderTargetView>());

            descriptorManager.Bind(context, renderer::DescriptorHeapType::SRV);

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.GetNativeCommandList().Get());

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &swapChain.GetCurrentBuffer().GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::RenderTarget,
                .To = spieler::renderer::ResourceState::Present
            });
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));
        SPIELER_RETURN_IF_FAILED(renderer.Present(renderer::VSyncState::Disabled));

        return true;
    }

    void Application::OnImGuiRender(float dt)
    {
        m_LayerStack.OnImGuiRender(dt);
    }

    void Application::WindowEventCallback(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowClose));
        dispatcher.Dispatch<WindowMinimizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMinimized));
        dispatcher.Dispatch<WindowMaximizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMaximized));
        dispatcher.Dispatch<WindowRestoredEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowRestored));
        dispatcher.Dispatch<WindowEnterResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowEnterResizing));
        dispatcher.Dispatch<WindowExitResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowExitResizing));
        dispatcher.Dispatch<WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<WindowFocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowFocused));
        dispatcher.Dispatch<WindowUnfocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowUnfocused));

        m_LayerStack.OnEvent(event);
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        m_ApplicationProps.IsRunning = false;

        return false;
    }

    bool Application::OnWindowMinimized(WindowMinimizedEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        return false;
    }

    bool Application::OnWindowMaximized(WindowMaximizedEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        return false;
    }

    bool Application::OnWindowRestored(WindowRestoredEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        return false;
    }

    bool Application::OnWindowEnterResizing(WindowEnterResizingEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        m_Timer.Stop();

        return false;
    }

    bool Application::OnWindowExitResizing(WindowExitResizingEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowResized(WindowResizedEvent& event)
    {
        renderer::Renderer& renderer{ renderer::Renderer::GetInstance() };
        renderer.ResizeBuffers(m_Window->GetWidth(), m_Window->GetHeight());

        UpdateScreenViewport();
        UpdateScreenScissorRect();

        return false;
    }

    bool Application::OnWindowFocused(WindowFocusedEvent& event)
    {
        m_ApplicationProps.IsPaused = false;

        m_Timer.Start();

        return false;
    }

    bool Application::OnWindowUnfocused(WindowUnfocusedEvent& event)
    {
        m_ApplicationProps.IsPaused = true;

        m_Timer.Stop();

        return false;
    }

    void Application::CalcStats(float dt)
    {
        const float fps{ 1.0f / dt };

        m_Window->SetTitle(fmt::format("FPS: {:.2f}, ms: {:.2f}", fps, dt * 1000.0f));
    }

    void Application::UpdateScreenViewport()
    {
        m_ScreenViewport.X = 0.0f;
        m_ScreenViewport.Y = 0.0f;
        m_ScreenViewport.Width = static_cast<float>(m_Window->GetWidth());
        m_ScreenViewport.Height = static_cast<float>(m_Window->GetHeight());
        m_ScreenViewport.MinDepth = 0.0f;
        m_ScreenViewport.MaxDepth = 1.0f;
    }

    void Application::UpdateScreenScissorRect()
    {
        m_ScreenScissorRect.X = 0.0f;
        m_ScreenScissorRect.Y = 0.0f;
        m_ScreenScissorRect.Width = static_cast<float>(m_Window->GetWidth());
        m_ScreenScissorRect.Height = static_cast<float>(m_Window->GetHeight());
    }

} // namespace spieler