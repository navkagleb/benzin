#pragma once

#include <benzin/core/interval_timer.hpp>
#include <benzin/graphics2/imgui_helpers.hpp> // TODO: Remove
#include <benzin/graphics2/render_pass.hpp>
#include <benzin/system/key_code.hpp>

#include <shaders/joint/imgui_resources.hpp>

namespace benzin
{

    class Descriptor;
    class Event;
    class GraphicsCmdList;
    class Window;

    class ImGuiTool
    {
    public:
        friend class ImGuiManager;

        ImGuiTool(std::string_view path, KeyCode shortcutKeyCode = KeyCode::Unknown);
        virtual ~ImGuiTool() = default;

        virtual void OnEvent(Event& event);
        virtual void DrawWindow();

    protected:
        virtual void DrawWindowContent() = 0;
        virtual void PostDrawWindow() {}

        void DrawWindow(ImGuiWindowFlags flags);

    protected:
        static inline const Window* ms_Window = nullptr;
        static inline const IntervalTimer* ms_IntervalTimer = nullptr;

        uint64_t m_UniqueId = 0;
        std::string_view m_Path;
        KeyCode m_ShortcutKeyCode;
        bool m_IsVisible = false;

        bool m_IsHovered = false;
        bool m_IsCollapsed = false;
    };

    // TODO: Move to math.hpp file or something like that
    constexpr uint64_t FNV1A(const char* str)
    {
        uint64_t hash = 14695981039346656037ull;
        for (uint32_t i = 0; str[i]; ++i)
        {
            hash ^= (uint8_t)str[i];
            hash *= 1099511628211ull;
        }

        return hash;
    }

    class ImGuiManager
    {
    public:
        friend class ImGuiPass;

        ImGuiManager(Window& window, const TickTimer& frameTimer);
        ~ImGuiManager();

    public:
        void BeginFrame();

        void BeginUiFrame() const;
        void EndUiFrame() const;

        void OnEvent(Event& event);
        void DrawUi();

        template <std::derived_from<ImGuiTool> ImGuiToolT, typename... Args>
        void RegisterTool(Args&&... args)
        {
            const uint64_t id = GetToolId<ImGuiToolT>();
            BenzinAssert(std::ranges::find_if(m_Tools, [id](const auto& existing) { return existing->m_UniqueId == id; }) == m_Tools.end());

            auto tool = std::make_unique<ImGuiToolT>(std::forward<Args>(args)...);
            tool->m_UniqueId = id;
            tool->m_IsVisible = m_ToolVisibilityCache[tool->m_UniqueId];

            m_Tools.push_back(std::move(tool));
            std::ranges::sort(m_Tools, {}, &ImGuiTool::m_Path);
        }

        template <std::derived_from<ImGuiTool> ImGuiToolT>
        void UnregisterTool()
        {
            const uint64_t id = GetToolId<ImGuiToolT>();
            BenzinAssert(std::ranges::find_if(m_Tools, [id](const auto& existing) { return existing->m_UniqueId == id; }) != m_Tools.end());

            std::erase_if(m_Tools, [id](const auto& tool) { return tool->m_UniqueId == id; });
        }

    private:
        template <std::derived_from<ImGuiTool> ImGuiToolT>
        uint64_t GetToolId()
        {
            return FNV1A(std::source_location::current().function_name());
        }

        void DrawDockSpace();
        void DrawDockSpaceContent();
        void DrawManuBar();
        void DrawToolMenuPath(ImGuiTool& tool, std::span<const std::string_view> pathParts, uint32_t depth);

        void ToggleImGuiDemoWindow();
        void ToggleUiDraw();

        void LoadToolVisiblityCache();
        void SaveToolVisiblityCache();

    private:
        IntervalTimer m_IntervalTimer;

        std::vector<std::unique_ptr<ImGuiTool>> m_Tools;
        std::unordered_map<uint64_t, bool> m_ToolVisibilityCache;

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
