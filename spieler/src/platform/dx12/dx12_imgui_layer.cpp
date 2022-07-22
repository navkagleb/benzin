#include "spieler/config/bootstrap.hpp"

#if defined(SPIELER_GRAPHICS_API_DX12)

#include "spieler/core/imgui_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/imgui/imgui_impl_dx12.h>
#include <third_party/imgui/imgui_impl_win32.h>

#include "spieler/core/application.hpp"
#include "spieler/core/common.hpp"
#include "spieler/core/logger.hpp"

#include "spieler/system/event_dispatcher.hpp"
#include "spieler/system/key_event.hpp"

#include "spieler/renderer/renderer.hpp"

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
        auto& renderer{ renderer::Renderer::GetInstance() };
        auto& context{ renderer.GetContext() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& descriptorManager{ renderer.GetDevice().GetDescriptorManager() };

        ImGui::Render();

        SPIELER_ASSERT(context.ResetCommandList());
        {
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &swapChain.GetCurrentBuffer().GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::RenderTarget }
            });

            context.SetRenderTarget(swapChain.GetCurrentBuffer().GetView<spieler::renderer::RenderTargetView>());

            descriptorManager.Bind(context, renderer::DescriptorHeapType::SRV);

            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), context.GetNativeCommandList().Get());

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &swapChain.GetCurrentBuffer().GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::RenderTarget },
                .To{ spieler::renderer::ResourceState::Present }
            });
        }
        SPIELER_ASSERT(context.ExecuteCommandList(true));
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

        auto& renderer{ renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& descriptorManager{ device.GetDescriptorManager() };

        renderer::SRVDescriptor fontDescriptor{ descriptorManager.AllocateSRV() };

        SPIELER_RETURN_IF_FAILED(ImGui_ImplWin32_Init(Application::GetInstance()->GetWindow().GetNativeHandle<::HWND>()));
        SPIELER_RETURN_IF_FAILED(ImGui_ImplDX12_Init(
            device.GetNativeDevice().Get(),
            1,
            renderer::dx12::Convert(swapChain.GetBufferFormat()),
            descriptorManager.GetDescriptorHeap(renderer::DescriptorHeapType::SRV).GetNative().Get(),
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

            return true;
        });
    }

} // namespace spieler

#endif // defined(SPIELER_GRAPHICS_API_DX12)
