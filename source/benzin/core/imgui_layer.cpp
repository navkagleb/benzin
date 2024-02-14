#include "benzin/config/bootstrap.hpp"
#include "benzin/core/imgui_layer.hpp"

#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/core/imgui_helper.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/texture.hpp"

namespace benzin
{

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
            GraphicsSettingsInstance::Get().FrameInFlightCount,
            (DXGI_FORMAT)GraphicsSettingsInstance::Get().BackBufferFormat,
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

        auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        commandList.SetResourceBarrier(TransitionBarrier{ currentBackBuffer, ResourceState::RenderTarget });

        commandList.SetRenderTargets({ currentBackBuffer.GetRenderTargetView() });
        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetD3D12GraphicsCommandList());

        commandList.SetResourceBarrier(TransitionBarrier{ currentBackBuffer, ResourceState::Present });
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        {
            // ImGui layer handle system events

            ImGuiIO& io = ImGui::GetIO();
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;
        }

        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch(&ImGuiLayer::OnKeyPressed, *this);
    }

    void ImGuiLayer::OnImGuiRender()
    {
        if (m_IsDemoWindowEnabled)
        {
            ImGui::ShowDemoWindow(&m_IsDemoWindowEnabled);
        }

        // BottomPanel
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f });
            BenzinExecuteOnScopeExit([&]{ ImGui::PopStyleVar(3); });

            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(184, 100, 0, 240));
            BenzinExecuteOnScopeExit([&] { ImGui::PopStyleColor(); });

            ImGuiContext& context = *ImGui::GetCurrentContext();
            const float panelHeight = context.FontBaseSize + context.Style.WindowPadding.y * 2.0f;

            ImGui::SetNextWindowPos(ImVec2{ 0.0f, context.IO.DisplaySize.y - panelHeight });
            ImGui::SetNextWindowSize(ImVec2{ context.IO.DisplaySize.x, panelHeight });

            const ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
            ImGui::Begin("Bottom Panel", nullptr, windowFlags);
            {
                ImGui_Text(
                    "{} | "
                    "FPS: {:.1f} ({:.3f} ms) | "
                    "Begin: {:.3f}, Process: {:.3f}, End: {:.3f} | "
                    "({} x {}) | "
                    "CPU: {}, GPU: {}, ActiveFrame: {}",
                    m_Backend.GetMainAdapterName(),
                    m_FrameRate, m_FrameDeltaTimeMS,
                    ToFloatMS(m_ApplicationTimings[ApplicationTiming::BeginFrame]), ToFloatMS(m_ApplicationTimings[ApplicationTiming::ProcessFrame]), ToFloatMS(m_ApplicationTimings[ApplicationTiming::EndFrame]),
                    m_Window.GetWidth(), m_Window.GetHeight(),
                    m_Device.GetCPUFrameIndex(), m_Device.GetGPUFrameIndex(), m_Device.GetActiveFrameIndex()
                );
            }
            ImGui::End();
        }
    }

    bool ImGuiLayer::OnKeyPressed(KeyPressedEvent& event)
    {
        switch (event.GetKeyCode())
        {
            case KeyCode::F1:
            {
                m_IsWidgetDrawingEnabled = !m_IsWidgetDrawingEnabled;
                break;
            }
            case KeyCode::O:
            {
                m_IsDemoWindowEnabled = !m_IsDemoWindowEnabled;
                break;
            }
            case KeyCode::V:
            {
                m_SwapChain.SetVSyncEnabled(!m_SwapChain.IsVSyncEnabled());
            }
        }

        return false;
    }

} // namespace benzin
