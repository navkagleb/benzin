#pragma once

#include "benzin/core/layer.hpp"

#include "benzin/system/window.hpp"

#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/command_queue.hpp"
#include "benzin/graphics/api/swap_chain.hpp"
#include "benzin/graphics/api/graphics_command_list.hpp"

namespace benzin
{

    class Window;

    class ImGuiLayer final : public Layer
    {
    public:
        ImGuiLayer(Window& window, Device& device, CommandQueue& commandQueue, SwapChain& swapChain);

    public:
        bool IsEventsAreBlocked() const { return m_IsEventsAreBlocked; }
        void SetIsEventsAreBlocked(bool isEventsAreBlocked) { m_IsEventsAreBlocked = isEventsAreBlocked; }

    public:
        bool OnAttach() override;
        bool OnDetach() override;

        void Begin();
        void End();

        void OnEvent(Event& event) override;
        void OnImGuiRender(float dt) override;

    private:
        Window& m_Window;
        Device& m_Device;
        CommandQueue& m_CommandQueue;
        SwapChain& m_SwapChain;

        Descriptor m_FontDescriptor;

        bool m_IsEventsAreBlocked{ true };
        bool m_IsDemoWindowEnabled{ false };
        bool m_IsBottomPanelEnabled{ true };
    };

} // namespace benzin
