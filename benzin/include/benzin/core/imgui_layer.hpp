#pragma once

#include "benzin/core/layer.hpp"

#include "benzin/system/window.hpp"

#include "benzin/graphics/device.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/graphics/graphics_command_list.hpp"

namespace benzin
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

} // namespace benzin
