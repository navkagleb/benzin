#include "benzin/config/bootstrap.hpp"
#include "benzin/core/imgui_layer.hpp"

#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    ImGuiLayer::ImGuiLayer(Window& window, Backend& backend, Device& device, SwapChain& swapChain)
        : m_Window{ window }
        , m_Backend{ backend }
        , m_Device{ device }
        , m_SwapChain{ swapChain }
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        //io.IniFilename = "config/imgui.ini";

        ImGui::StyleColorsClassic();

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGuiStyle& style = ImGui::GetStyle();
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        m_FontDescriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::ShaderResourceView);

        BenzinAssert(ImGui_ImplWin32_Init(m_Window.GetWin64Window()));
        BenzinAssert(ImGui_ImplDX12_Init(
            m_Device.GetD3D12Device(),
            config::g_BackBufferCount,
            static_cast<DXGI_FORMAT>(config::g_BackBufferFormat),
            m_Device.GetDescriptorManager().GetD3D12ResourceDescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetCPUHandle() },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetGPUHandle() }
        ));
    }

    ImGuiLayer::~ImGuiLayer()
    {
        m_Device.GetDescriptorManager().DeallocateDescriptor(DescriptorType::ShaderResourceView, m_FontDescriptor);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::Begin()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        ImGui::Render();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }

        {
            auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

            commandList.SetResourceBarrier(*m_SwapChain.GetCurrentBackBuffer(), ResourceState::RenderTarget);

            commandList.SetRenderTargets({ m_SwapChain.GetCurrentBackBuffer()->GetRenderTargetView() });
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetD3D12GraphicsCommandList());

            commandList.SetResourceBarrier(*m_SwapChain.GetCurrentBackBuffer(), ResourceState::Present);
        }
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (m_IsEventsAreBlocked)
        {
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;
        }

        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch(&ImGuiLayer::OnKeyPressed, *this);
    }

    void ImGuiLayer::OnImGuiRender()
    {
        static float currentFramerate = ImGui::GetIO().Framerate;

        if (m_SwapChain.GetGPUFrameIndex() % 30 == 0)
        {
            currentFramerate = ImGui::GetIO().Framerate;
        }

        if (m_IsDemoWindowEnabled)
        {
            ImGui::ShowDemoWindow(&m_IsDemoWindowEnabled);
        }

        if (m_IsBottomPanelEnabled)
        {
            ImGuiContext& context = *ImGui::GetCurrentContext();
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f });
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(184, 100, 0, 240));

            const float panelHeight = context.FontBaseSize + context.Style.WindowPadding.y * 2.0f;
            ImGui::SetNextWindowPos(ImVec2{ 0.0f, context.IO.DisplaySize.y - panelHeight });
            ImGui::SetNextWindowSize(ImVec2{ context.IO.DisplaySize.x, panelHeight });

            const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
            if (ImGui::Begin("Bottom Panel", &m_IsBottomPanelEnabled, windowFlags))
            {
                ImGui::Text(
                    "%s | "
                    "FPS: %.1f (%.3f ms) | "
                    "(%d x %d)",
                    m_Backend.GetMainAdapterName().c_str(),
                    currentFramerate, 1000.0f / currentFramerate,
                    m_Window.GetWidth(), m_Window.GetHeight()
                );
                ImGui::End();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
        }
    }

    bool ImGuiLayer::OnKeyPressed(KeyPressedEvent& event)
    {
        if (event.GetKeyCode() == KeyCode::F1)
        {
            m_IsWidgetDrawingEnabled = !m_IsWidgetDrawingEnabled;
        }

        if (event.GetKeyCode() == KeyCode::I)
        {
            m_IsEventsAreBlocked = !m_IsEventsAreBlocked;
        }

        if (event.GetKeyCode() == KeyCode::O)
        {
            m_IsDemoWindowEnabled = !m_IsDemoWindowEnabled;
        }

        if (event.GetKeyCode() == KeyCode::P)
        {
            m_IsBottomPanelEnabled = !m_IsBottomPanelEnabled;
        }

        return false;
    }

} // namespace benzin
