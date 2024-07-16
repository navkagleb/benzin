#pragma once

#include "benzin/engine/render_pass.hpp"
#include "benzin/graphics/descriptor_manager.hpp"

namespace benzin
{

    class Device;
    class Event;
    class Window;

    class ImGuiTool
    {
    public:
        friend class ImGuiManager;
        friend class ImGuiPass;

        ImGuiTool(std::string_view name, bool isVisible);
        virtual ~ImGuiTool() = default;

    public:
        virtual void OnEvent(Event& event) { BenzinUnused(event); };
        virtual void SpawnImGui() = 0;

    protected:
        void SpawnImGuiWindow(const std::function<void()>& callback);

    protected:
        std::string_view m_Name;
        bool m_IsVisible = false;
    };

    class ImGuiManager
    {
    public:
        friend class ImGuiPass;

        ImGuiManager(const Window& window, Device& device);
        ~ImGuiManager();

    public:
        void BeginUiFrame() const;
        void EndUiFrame() const;

        void OnEvent(Event& event);
        void SpawnUi();

        template <std::derived_from<ImGuiTool> T, typename... Args>
        T* PushTool(Args&&... args)
        {
            auto* tool = new T{ std::forward<Args>(args)... };

            m_Tools.push_back(tool);
            std::ranges::sort(m_Tools, {}, &ImGuiTool::m_Name);

            return tool;
        }

    private:
        void SpawnImGuiDockSpace(const std::function<void()>& callback);
        void SpawnImGuiManuBar();

    private:
        Device& m_Device;

        Descriptor m_FontDescriptor;
        std::vector<ImGuiTool*> m_Tools;

        bool m_IsDemoWindowVisible = false;
        bool m_IsSpawnEnabled = true;

        mutable ImDrawData* m_CurrentImGuiDrawData = nullptr;
    };

    class ImGuiPass : public RenderPass
    {
    public:
        ImGuiPass(ImGuiManager& imGuiManager, uint32_t imGuiTextureKey, uint32_t gpuTimingIndex);

        auto GetCpuRenderTime() const { return m_CpuRenderTime; }

        void OnWindowResize(uint32_t width, uint32_t height) override;
        void OnRender() const override;

    private:
        ImGuiManager& m_ImGuiManager;
        
        uint32_t m_ImGuiTextureKey;
        uint32_t m_GpuTimingIndex;

        mutable std::chrono::microseconds m_CpuRenderTime = std::chrono::microseconds::zero();
    };

}
