#pragma once

#include <shaders/joint/imgui_resources.hpp>

#include "benzin/graphics2/render_pass.hpp"

namespace benzin
{

    class Descriptor;
    class Device;
    class Event;
    class GraphicsCommandList;
    class Window;

    class ImGuiTool
    {
    public:
        friend class ImGuiManager;

        ImGuiTool(std::string_view name, std::string_view shortcut = {});
        virtual ~ImGuiTool() = default;

    public:
        virtual void OnEvent(Event& event) { BenzinUnused(event); };
        virtual void SpawnImGui() = 0;

    protected:
        void SpawnImGuiWindow(const std::function<void()>& callback);
        void SpawnImGuiWindow(ImGuiWindowFlags flags, const std::function<void()>& callback);
        bool SpawnImGuiCollapsingHeader(std::string_view name, bool isOpenByDefault = true) const;

    protected:
        static inline const Window* ms_Window = nullptr;
        static inline const TickTimer* ms_FrameTimer = nullptr; // TODO: Ugly solution

        std::string_view m_Name;
        std::string_view m_Shortcut;
        bool m_IsVisible = false;
    };

    class ImGuiManager
    {
    public:
        friend class ImGuiPass;

        ImGuiManager(Window& window, Device& device, const TickTimer& frameTimer);
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
            tool->m_IsVisible = m_IsToolVisibleMap[tool->m_Name.data()];

            m_Tools.push_back(tool);
            std::ranges::sort(m_Tools, {}, &ImGuiTool::m_Name);

            return tool;
        }

        void PushSpawnImGuiMenuCallback(std::function<void()>&& callback);

    private:
        const ImDrawData& GetImDrawData() const { BenzinAssert(m_CurrentImGuiDrawData != nullptr); return *m_CurrentImGuiDrawData; }

        void SpawnImGuiDockSpace(const std::function<void()>& callback);
        void SpawnImGuiManuBar();

        void ToggleImGuiDemoWindow();
        void ToggleUiSpawn();

        void SaveToolsVisiblity();
        void LoadToolsVisiblity();

    private:
        Device& m_Device;

        std::unordered_map<std::string, bool> m_IsToolVisibleMap; // TODO: can std::string_view be used instead of std::string

        std::vector<ImGuiTool*> m_Tools;
        std::vector<std::function<void()>> m_ImGuiSpawnMenuCallbacks;

        bool m_IsImGuiDemoWindowVisible = false;
        bool m_IsUiSpawnEnabled = true;

        mutable ImDrawData* m_CurrentImGuiDrawData = nullptr;
    };

    class ImGuiPass : public RenderPass
    {
    public:
        static uint64_t PackImTextureId(const Descriptor& viewDescriptor, joint::ImGuiSamplerIndex samplerIndex);

    public:
        ImGuiPass(ImGuiManager& imGuiManager, uint32_t psoIndex);

    private:
        bool IsDependentOnViewport() const override { return false; }

        void OnZeroFrameInit() override;
        void OnUpdate() override;
        void OnRender() const override;

        void UploadFontTexture();
        void UpdateConsts(const ImDrawData& imDrawData);
        void UpdateVertexAndIndexBuffers(const ImDrawData& imDrawData);
        void RenderImDrawData(GraphicsCommandList& commandList) const;

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
        uint32_t m_PsoIndex;
    };

}
