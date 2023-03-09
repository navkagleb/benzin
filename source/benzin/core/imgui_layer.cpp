#include "benzin/config/bootstrap.hpp"

#include "benzin/core/imgui_layer.hpp"

#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/core/common.hpp"

#include "benzin/system/event_dispatcher.hpp"
#include "benzin/system/key_event.hpp"

#include "benzin/engine/camera.hpp"

namespace benzin
{

    ImGuiLayer::ImGuiLayer(Window& window, Device& device, CommandQueue& commandQueue, SwapChain& swapChain)
        : m_Window{ window }
        , m_Device{ device }
        , m_CommandQueue{ commandQueue }
        , m_SwapChain{ swapChain }
    {}

    bool ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        auto& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        //io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        //io.IniFilename = "config/imgui.ini";

        ImGui::StyleColorsClassic();

        auto& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        m_FontDescriptor = m_Device.GetDescriptorManager().AllocateDescriptor(Descriptor::Type::ShaderResourceView);

        BENZIN_ASSERT(ImGui_ImplWin32_Init(m_Window.GetWin64Window()));
        BENZIN_ASSERT(ImGui_ImplDX12_Init(
            m_Device.GetD3D12Device(),
            config::GetBackBufferCount(),
            static_cast<DXGI_FORMAT>(config::GetBackBufferFormat()),
            m_Device.GetDescriptorManager().GetD3D12ResourceDescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetCPUHandle() },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetGPUHandle() }
        ));

        return true;
    }

    bool ImGuiLayer::OnDetach()
    {
        m_Device.GetDescriptorManager().DeallocateDescriptor(Descriptor::Type::ShaderResourceView, m_FontDescriptor);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return true;
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
            auto& commandList = m_CommandQueue.GetGraphicsCommandList();

            commandList.SetResourceBarrier(*m_SwapChain.GetCurrentBackBuffer(), Resource::State::RenderTarget);

            commandList.SetRenderTarget(&m_SwapChain.GetCurrentBackBuffer()->GetRenderTargetView());
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetD3D12GraphicsCommandList());

            commandList.SetResourceBarrier(*m_SwapChain.GetCurrentBackBuffer(), Resource::State::Present);
        }
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        ImGuiIO& io = ImGui::GetIO();

        if (m_IsEventsAreBlocked)
        {
            event.m_IsHandled |= event.IsInCategory(EventCategory_Keyboard) & io.WantCaptureKeyboard;
            event.m_IsHandled |= event.IsInCategory(EventCategory_Mouse) & io.WantCaptureMouse;
        }

        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent event)
        {
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
        });
    }

    void ImGuiLayer::OnImGuiRender(float dt)
    {
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
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                ImGui::End();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
        }
    }

} // namespace benzin
