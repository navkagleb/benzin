#pragma once

#include <benzin/core/interval_timer.hpp>
#include <benzin/graphics2/imgui_helpers.hpp>
#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/imgui_resources.hpp>

namespace benzin
{

    class Descriptor;
    class Device;
    class Event;
    class GraphicsCmdList;
    class Window;

    class ImGuiTool
    {
    public:
        friend class ImGuiManager;

        ImGuiTool(std::string_view path, std::string_view shortcut = {});
        virtual ~ImGuiTool() = default;

        virtual void OnEvent(Event& event) { BenzinUnused(event); };
        virtual void DrawWindow();

    protected:
        virtual void DrawWindowContent() = 0;

        void DrawWindow(ImGuiWindowFlags flags);

    protected:
        static inline const Window* ms_Window = nullptr;
        static inline const IntervalTimer* ms_IntervalTimer = nullptr;

        std::string_view m_Path;
        std::string_view m_Shortcut;
        bool m_IsVisible = false;

        bool m_IsHovered = false;
        bool m_IsCollapsed = false;
    };

    class ImGuiManager
    {
    public:
        friend class ImGuiPass;

        ImGuiManager(Window& window, Device& device, const TickTimer& frameTimer);
        ~ImGuiManager();

    public:
        void BeginFrame();

        void BeginUiFrame() const;
        void EndUiFrame() const;

        void OnEvent(Event& event);
        void DrawUi();

        template <std::derived_from<ImGuiTool> T, typename... Args>
        T* PushTool(Args&&... args)
        {
            auto* tool = new T{ std::forward<Args>(args)... };
            tool->m_IsVisible = m_IsToolVisibleMap[tool->m_Path.data()];

            m_Tools.push_back(tool);
            std::ranges::sort(m_Tools, {}, &ImGuiTool::m_Path);

            return tool;
        }

        void AddDrawMenuCallback(ImGui::DrawCallback&& callback);

    private:
        const ImDrawData& GetImDrawData() const { BenzinAssert(m_CurrentImGuiDrawData != nullptr); return *m_CurrentImGuiDrawData; }

        void DrawDockSpace();
        void DrawDockSpaceContent();
        void DrawManuBar();
        void DrawToolMenuPath(ImGuiTool* tool, std::span<const std::string_view> pathParts, uint32_t depth = 0);

        void ToggleImGuiDemoWindow();
        void ToggleUiDraw();

        void SaveToolsVisiblity();
        void LoadToolsVisiblity();

    private:
        Device& m_Device;

        IntervalTimer m_IntervalTimer;

        std::unordered_map<std::string, bool> m_IsToolVisibleMap; // TODO: can std::string_view be used instead of std::string

        std::vector<ImGuiTool*> m_Tools;
        std::vector<ImGui::DrawCallback> m_DrawMenuCallbacks;

        bool m_IsImGuiDemoWindowVisible = false;
        bool m_IsUiDrawEnabled = true;

        mutable ImDrawData* m_CurrentImGuiDrawData = nullptr;
    };

    class ImGuiPass : public RenderPass
    {
    public:
        static uint64_t PackImTextureId(const Descriptor& viewDescriptor, joint::ImGuiSamplerIndex samplerIndex);

    public:
        explicit ImGuiPass(ImGuiManager& imGuiManager);
        ~ImGuiPass();

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

        void UploadFontTexture();
        void UpdateConsts(const ImDrawData& imDrawData);
        void UpdateVertexAndIndexBuffers(const ImDrawData& imDrawData);
        void RenderImDrawData(GraphicsCmdList& cmdList) const;

        void GetImGuiResources(const ImDrawCmd& imDrawCmd, uint32_t& outTextureSrvHeapIndex, joint::ImGuiSamplerIndex& outSamplerIndex) const;

    private:
        struct FrameContext
        {
            std::unique_ptr<Buffer> m_VertexBuffer;
            std::unique_ptr<Buffer> m_IndexBuffer;
        };

        ImGuiManager& m_ImGuiManager;

        std::vector<FrameContext> m_FrameContexts;
        std::unique_ptr<Texture> m_FontTexture;

        joint::ImGuiConsts m_Consts{};
    };

}
