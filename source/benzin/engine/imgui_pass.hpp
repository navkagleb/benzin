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

    public:
        virtual void OnEvent(Event& event) { BenzinUnused(event); };
        virtual void OnImGuiRender() = 0;

    protected:
        void RenderImGuiWindow(const std::function<void()>& callback);

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
        void OnEvent(Event& event);

        template <std::derived_from<ImGuiTool> T, typename... Args>
        T* PushTool(Args&&... args)
        {
            auto& tool = m_Tools.emplace_back(std::make_unique<T>(std::forward<Args>(args)...));
            
            std::ranges::sort(m_Tools, [](const auto& lhs, const auto& rhs)
            {
                return lhs->m_Name < rhs->m_Name;
            });

            return (T*)tool.get();
        }

    private:
        Device& m_Device;

        Descriptor m_FontDescriptor;
        std::vector<std::unique_ptr<ImGuiTool>> m_Tools;

        bool m_IsDemoWindowVisible = false;
    };

    class ImGuiPass : public RenderPass
    {
    public:
        ImGuiPass(ImGuiManager& imGuiManager, uint32_t finalOutputTextureKey, uint32_t gpuTimingIndex);

        auto GetCpuRenderTime() const { return m_CpuRenderTime; }

        void OnRender() const override;

    private:
        void Begin() const;
        void End() const;

    private:
        ImGuiManager& m_ImGuiManager;
        
        uint32_t m_FinalOutputTextureKey;
        uint32_t m_GpuTimingIndex;

        mutable std::chrono::microseconds m_CpuRenderTime;
    };

}
