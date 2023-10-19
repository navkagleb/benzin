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

    class FrameStats
    {
    public:
        float GetFrameRate() const { return m_FrameRate; }
        float GetDeltaTimeMS() const { return 1000.0f / m_FrameRate; }

        void OnUpdate(float dt);

    private:
        static constexpr float ms_ElapsedItervalInSeconds = 1.0f;

        uint32_t m_ElapsedFrameCount = 0;
        float m_ElapsedTimeInSeconds = 0.0f;
        float m_FrameRate = 0.0f;
    };

    class ImGuiLayer : public Layer
    {
    public:
        explicit ImGuiLayer(const GraphicsRefs& graphicsRefs);
        ~ImGuiLayer();

    public:
        bool IsWidgetDrawEnabled() const { return m_IsWidgetDrawingEnabled; }
        bool IsEventsAreBlocked() const { return m_IsEventsAreBlocked; }
        void SetIsEventsAreBlocked(bool isEventsAreBlocked) { m_IsEventsAreBlocked = isEventsAreBlocked; }

    public:
        void Begin();
        void End();

        void OnEvent(Event& event) override;
        void OnUpdate(float dt) override;
        void OnImGuiRender() override;

    private:
        bool OnKeyPressed(KeyPressedEvent& event);

    private:
        Window& m_Window;
        Backend& m_Backend;
        Device& m_Device;
        SwapChain& m_SwapChain;

        Descriptor m_FontDescriptor;

        FrameStats m_FrameStats;

        bool m_IsWidgetDrawingEnabled = false;
        bool m_IsEventsAreBlocked = true;
        bool m_IsDemoWindowEnabled = false;
        bool m_IsBottomPanelEnabled = true;
    };

} // namespace benzin
