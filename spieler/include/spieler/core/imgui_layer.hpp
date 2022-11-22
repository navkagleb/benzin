#pragma once

#include "spieler/core/layer.hpp"

#include "spieler/system/window.hpp"

#include "spieler/graphics/device.hpp"
#include "spieler/graphics/command_queue.hpp"
#include "spieler/graphics/swap_chain.hpp"
#include "spieler/graphics/graphics_command_list.hpp"

namespace spieler
{

    class Window;

    class Device;
    class CommandQueue;
    class SwapChain;

    class Camera;

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

    public:
        void SetCamera(Camera* camera);

    private:
        Window& m_Window;
        Device& m_Device;
        CommandQueue& m_CommandQueue;
        SwapChain& m_SwapChain;

        GraphicsCommandList m_GraphicsCommandList;

        bool m_IsEventsAreBlocked{ true };
        bool m_IsDemoWindowEnabled{ false };
        bool m_IsBottomPanelEnabled{ true };

        Camera* m_Camera{ nullptr };
    };

} // namespace spieler
