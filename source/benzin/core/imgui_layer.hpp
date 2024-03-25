#pragma once

#include "benzin/core/layer.hpp"
#include "benzin/graphics/descriptor_manager.hpp"
#include "benzin/system/key_event.hpp"

namespace benzin
{

    class Backend;
    class Buffer;
    class Device;
    class SwapChain;
    class Window;

    struct GraphicsRefs;

    enum class ApplicationTiming : uint32_t
    {
        BeginFrame,
        ProcessFrame,
        EndFrame,
    };

    using ApplicationTimings = magic_enum::containers::array<ApplicationTiming, std::chrono::microseconds>;

    class ImGuiLayer : public Layer
    {
    public:
        explicit ImGuiLayer(const GraphicsRefs& graphicsRefs);
        ~ImGuiLayer();

    public:
        bool IsWidgetDrawEnabled() const { return m_IsWidgetDrawingEnabled; }

        void SetFrameRateStats(float frameRate, float dt) { m_FrameRate = frameRate; m_FrameDeltaTimeMS = dt; }
        void SetApplicationTimings(const ApplicationTimings& timings) { m_ApplicationTimings = timings; }

    public:
        void Begin();
        void End(bool isNeedToClearBackBuffer);

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

        bool m_IsWidgetDrawingEnabled = true;
        bool m_IsImGuiHandleEvents = true;
        bool m_IsDemoWindowEnabled = false;

        float m_FrameRate = 0.0f;
        float m_FrameDeltaTimeMS = 0.0f;
        ApplicationTimings m_ApplicationTimings = { std::chrono::microseconds::zero() };
    };

} // namespace benzin
