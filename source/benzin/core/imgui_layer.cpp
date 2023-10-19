#include "benzin/config/bootstrap.hpp"
#include "benzin/core/imgui_layer.hpp"

#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

    // FrameStats
    void FrameStats::OnUpdate(float dt)
    {
        m_ElapsedFrameCount++;
        m_ElapsedTimeInSeconds += dt;

        if (m_ElapsedTimeInSeconds >= ms_ElapsedItervalInSeconds)
        {
            m_FrameRate = static_cast<float>(m_ElapsedFrameCount) / m_ElapsedTimeInSeconds;

            m_ElapsedFrameCount = 0;
            m_ElapsedTimeInSeconds -= ms_ElapsedItervalInSeconds;
        }
    }

    // ImGuiLayer
    ImGuiLayer::ImGuiLayer(const GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Backend{ graphicsRefs.BackendRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

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

    void ImGuiLayer::OnUpdate(float dt)
    {
        m_FrameStats.OnUpdate(dt);
    }

    void ImGuiLayer::OnImGuiRender()
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
                const std::string text = fmt::format(
                    "{} | "
                    "FPS: {:.1f} ({:.3f} ms) | "
                    "({} x {})",
                    m_Backend.GetMainAdapterName(),
                    m_FrameStats.GetFrameRate(), m_FrameStats.GetDeltaTimeMS(),
                    m_Window.GetWidth(), m_Window.GetHeight()
                );

                ImGui::Text(text.c_str());
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
