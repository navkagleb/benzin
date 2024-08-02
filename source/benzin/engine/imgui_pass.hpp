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

        ImGuiTool(std::string_view name, bool isVisible);
        virtual ~ImGuiTool() = default;

    public:
        auto IsVisible() const { return m_IsVisible; }

        virtual void OnEvent(Event& event) { BenzinUnused(event); };
        virtual void SpawnImGui() = 0;

    protected:
        void SpawnImGuiWindow(const std::function<void()>& callback);
        bool SpawnImGuiCollapsingHeader(std::string_view name, bool isOpenByDefault = true) const;

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

        void PushSpawnImGuiMenuCallback(std::function<void()>&& callback);

    private:
        void SpawnImGuiDockSpace(const std::function<void()>& callback);
        void SpawnImGuiManuBar();

        void ToggleImGuiDemoWindow();
        void ToggleUiSpawn();

    private:
        Device& m_Device;

        Descriptor m_FontDescriptor;
        std::vector<ImGuiTool*> m_Tools;
        std::vector<std::function<void()>> m_ImGuiSpawnMenuCallbacks;

        bool m_IsImGuiDemoWindowVisible = false;
        bool m_IsUiSpawnEnabled = true;

        mutable ImDrawData* m_CurrentImGuiDrawData = nullptr;
    };

    class ImGuiPass : public RenderPass
    {
    public:
        ImGuiPass(ImGuiManager& imGuiManager, uint32_t imGuiTextureIndex, uint32_t gpuTimingIndex);
        ~ImGuiPass() override;

        bool IsDependentOnViewport() const override { return false; }

        auto GetCpuRenderTime() const { return m_CpuRenderTime; }

        void OnWindowResize(uint32_t width, uint32_t height) override;
        void OnRender() const override;

    private:
        ImGuiManager& m_ImGuiManager;
        
        uint32_t m_ImGuiTextureIndex;
        uint32_t m_GpuTimingIndex;

        mutable std::chrono::microseconds m_CpuRenderTime = std::chrono::microseconds::zero();
    };

}
