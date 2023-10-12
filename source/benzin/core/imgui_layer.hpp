#pragma once

#include "benzin/core/layer.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    class ImGuiLayer final : public Layer
    {
    public:
        ImGuiLayer(Window& window, Backend& backend, Device& device, SwapChain& swapChain);
        ~ImGuiLayer();

    public:
        bool IsWidgetDrawEnabled() const { return m_IsWidgetDrawingEnabled; }
        bool IsEventsAreBlocked() const { return m_IsEventsAreBlocked; }
        void SetIsEventsAreBlocked(bool isEventsAreBlocked) { m_IsEventsAreBlocked = isEventsAreBlocked; }

    public:
        void Begin();
        void End();

        void OnEvent(Event& event) override;
        void OnImGuiRender() override;

    private:
        bool OnKeyPressed(KeyPressedEvent& event);

    private:
        Window& m_Window;
        Backend& m_Backend;
        Device& m_Device;
        SwapChain& m_SwapChain;

        Descriptor m_FontDescriptor;

        bool m_IsWidgetDrawingEnabled = false;
        bool m_IsEventsAreBlocked = true;
        bool m_IsDemoWindowEnabled = false;
        bool m_IsBottomPanelEnabled = true;
    };

} // namespace benzin
