#pragma once

#include "benzin/core/layer.hpp"
#include "benzin/graphics/api/descriptor_manager.hpp"
#include "benzin/graphics/api/device.hpp"
#include "benzin/graphics/api/swap_chain.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    class Window;

    class ImGuiLayer final : public Layer
    {
    public:
        ImGuiLayer(Window& window, Device& device, SwapChain& swapChain);

    public:
        bool IsWidgetDrawEnabled() const { return m_IsWidgetDrawingEnabled; }
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
        SwapChain& m_SwapChain;

        Descriptor m_FontDescriptor;

        bool m_IsWidgetDrawingEnabled{ false };
        bool m_IsEventsAreBlocked{ true };
        bool m_IsDemoWindowEnabled{ false };
        bool m_IsBottomPanelEnabled{ true };
    };

} // namespace benzin
