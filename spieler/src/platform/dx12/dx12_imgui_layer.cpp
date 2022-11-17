#include "spieler/config/bootstrap.hpp"

#if defined(SPIELER_GRAPHICS_API_DX12)

#include "spieler/core/imgui_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/imgui_impl_dx12.h>
#include <third_party/imgui/imgui_impl_win32.h>

#include "spieler/core/application.hpp"
#include "spieler/core/common.hpp"

#include "spieler/system/event_dispatcher.hpp"
#include "spieler/system/key_event.hpp"

#include "spieler/graphics/renderer.hpp"
#include "spieler/engine/camera.hpp"

#include "platform/dx12/dx12_common.hpp"

namespace spieler
{

    void ImGuiLayer::Begin()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        auto& renderer{ Renderer::GetInstance() };
        auto& context{ renderer.GetContext() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& descriptorManager{ renderer.GetDevice().GetDescriptorManager() };

        ImGui::Render();

        {
            Context::CommandListScope commandListScope{ context, true };

            context.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ swapChain.GetCurrentBuffer().GetTextureResource().get() },
                .From{ spieler::ResourceState::Present },
                .To{ spieler::ResourceState::RenderTarget }
            });

            context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(DescriptorHeap::Type::SRV));
            context.SetRenderTarget(swapChain.GetCurrentBuffer().GetView<TextureRenderTargetView>());

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.GetDX12GraphicsCommandList());

            context.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ swapChain.GetCurrentBuffer().GetTextureResource().get() },
                .From{ spieler::ResourceState::RenderTarget },
                .To{ spieler::ResourceState::Present }
            });
        }
    }

    bool ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        auto& io{ ImGui::GetIO() };
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        io.IniFilename = "config/imgui.ini";

        ImGui::StyleColorsClassic();

        auto& renderer{ Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& descriptorManager{ device.GetDescriptorManager() };

        SRVDescriptor fontDescriptor{ descriptorManager.AllocateSRV() };

        SPIELER_RETURN_IF_FAILED(ImGui_ImplWin32_Init(Application::GetInstance().GetWindow().GetNativeHandle<::HWND>()));
        SPIELER_RETURN_IF_FAILED(ImGui_ImplDX12_Init(
            device.GetDX12Device(),
            1,
            dx12::Convert(swapChain.GetBufferFormat()),
            descriptorManager.GetDescriptorHeap(DescriptorHeap::Type::SRV).GetDX12DescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ fontDescriptor.CPU },
            D3D12_GPU_DESCRIPTOR_HANDLE{ fontDescriptor.GPU }
        ));

        return true;
    }

    bool ImGuiLayer::OnDetach()
    {
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();

        return true;
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        ImGuiIO& io{ ImGui::GetIO() };

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
            ImGuiContext& context{ *ImGui::GetCurrentContext() };
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f });
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(184, 100, 0, 240));

            const float panelHeight{ context.FontBaseSize + context.Style.WindowPadding.y * 2.0f };
            ImGui::SetNextWindowPos(ImVec2{ 0.0f, context.IO.DisplaySize.y - panelHeight });
            ImGui::SetNextWindowSize(ImVec2{ context.IO.DisplaySize.x, panelHeight });

            if (ImGui::Begin("Bottom Panel", &m_IsBottomPanelEnabled, ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs))
            {
                ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
                
                if (m_Camera)
                {
                    ImGui::SameLine();
                    ImGui::Text(
                        "  |  Camera Position: { %.2f, %.2f, %.2f }", 
                        DirectX::XMVectorGetX(m_Camera->GetPosition()),
                        DirectX::XMVectorGetY(m_Camera->GetPosition()),
                        DirectX::XMVectorGetZ(m_Camera->GetPosition())
                    );
                }
                
                ImGui::End();
            }

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(3);
        }
    }

    void ImGuiLayer::SetCamera(Camera* camera)
    {
        m_Camera = camera;
    }

} // namespace spieler

#endif // defined(SPIELER_GRAPHICS_API_DX12)
