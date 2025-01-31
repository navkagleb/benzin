#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/imgui_pass.hpp"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics2/gpu_profiler.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    static const auto g_ToolsVisiblityPath = std::filesystem::absolute("bin/tools_visiblity.txt");

    // ImGuiTool

    ImGuiTool::ImGuiTool(std::string_view name, std::string_view shortcut)
        : m_Name{ name }
        , m_Shortcut{ shortcut }
    {}
    
    void ImGuiTool::SpawnImGuiWindow(const std::function<void()>& callback)
    {
        SpawnImGuiWindow(ImGuiWindowFlags_None, callback);
    }

    void ImGuiTool::SpawnImGuiWindow(ImGuiWindowFlags flags, const std::function<void()>& callback)
    {
        if (ImGui::Begin(m_Name.data(), &m_IsVisible, flags))
        {
            callback();
        }
        ImGui::End();
    }

    bool ImGuiTool::SpawnImGuiCollapsingHeader(std::string_view name, bool isOpenByDefault) const
    {
        constexpr ImVec4 headerColor{ 0.7f, 1.0f, 0.7f, 1.0f };
        constexpr ImVec4 headerBackground{ 0.7f * 0.3f, 1.0f * 0.3f, 0.7f * 0.3f, 1.0f };

        ImGui::PushStyleColor(ImGuiCol_Text, headerColor);
        ImGui::PushStyleColor(ImGuiCol_Header, headerBackground);
        BenzinExecuteOnScopeExit([]{ ImGui::PopStyleColor(2); });

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_CollapsingHeader;
        if (isOpenByDefault)
        {
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        }

        return ImGui::CollapsingHeader(name.data(), flags);
    }

    // ImGuiManager

    ImGuiManager::ImGuiManager(const Window& window, Device& device)
        : m_Device{ device }
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // TODO: Mouse events are broken

        ImGui::StyleColorsDark();

        BenzinEnsure(ImGui_ImplWin32_Init(window.GetWin64Window()));

        m_LegacySigleSrvDescriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv);

        ImGui_ImplDX12_InitInfo imguiInitInfo;
        imguiInitInfo.Device = m_Device.GetD3D12Device();
        imguiInitInfo.CommandQueue = m_Device.GetGraphicsCommandQueue().GetD3D12CommandQueue();
        imguiInitInfo.NumFramesInFlight = CommandLineArgs::GetU32("FrameInFlightCount");
        imguiInitInfo.RTVFormat = (DXGI_FORMAT)CommandLineArgs::GetU32("BackBufferFormat");
        imguiInitInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
        imguiInitInfo.SrvDescriptorHeap = m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap();
        imguiInitInfo.LegacySingleSrvCpuDescriptor.ptr = m_LegacySigleSrvDescriptor.GetCpuHandle();
        imguiInitInfo.LegacySingleSrvGpuDescriptor.ptr = m_LegacySigleSrvDescriptor.GetGpuHandle();

        BenzinEnsure(ImGui_ImplDX12_Init(&imguiInitInfo));

        {
            // Force call 'ImGui_ImplDX12_CreateDeviceObjects' to copy
            // m_LegacySigleSrvDescriptor from CPU descriptor heap to GPU descriptor heap

            ImGui_ImplDX12_CreateDeviceObjects();
            m_Device.GetDescriptorManager().CopyToGpuResourceHeap(m_LegacySigleSrvDescriptor);
        }

        ImGuiTool::ms_Window = &window;

        LoadToolsVisiblity();
    }

    ImGuiManager::~ImGuiManager()
    {
        SaveToolsVisiblity();

        for (auto* tool : m_Tools)
        {
            delete tool;
        }
        m_Tools.clear();

        m_Device.GetDescriptorManager().FreeDescriptor(m_LegacySigleSrvDescriptor);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiManager::BeginUiFrame() const
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        m_CurrentImGuiDrawData = nullptr;
    }

    void ImGuiManager::EndUiFrame() const
    {
        ImGui::EndFrame();
        ImGui::Render();

        m_CurrentImGuiDrawData = ImGui::GetDrawData();

        const ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiManager::OnEvent(Event& event)
    {
        const EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<KeyPressedEvent>([this](const auto& event)
        {
            switch (event.GetKeyCode())
            {
                case KeyCode::O:
                {
                    ToggleImGuiDemoWindow();
                    break;
                }
                case KeyCode::F1:
                {
                    ToggleUiSpawn();
                    break;
                }
            }

            return false;
        });

        // ImGuiManager handles system events
        const ImGuiIO& io = ImGui::GetIO();
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;

        for (auto& tool : m_Tools)
        {
            tool->OnEvent(event);
        }
    }

    void ImGuiManager::SpawnUi()
    {
        if (!m_IsUiSpawnEnabled)
        {
            return;
        }

        SpawnImGuiDockSpace([this]
        {
            SpawnImGuiManuBar();

            if (m_IsImGuiDemoWindowVisible)
            {
                ImGui::ShowDemoWindow(&m_IsImGuiDemoWindowVisible);
            }

            for (auto* tool : m_Tools)
            {
                if (tool->m_IsVisible)
                {
                    tool->SpawnImGui();
                }
            };
        });
    }

    void ImGuiManager::PushSpawnImGuiMenuCallback(std::function<void()>&& callback)
    {
        m_ImGuiSpawnMenuCallbacks.push_back(std::move(callback));
    }

    void ImGuiManager::SpawnImGuiDockSpace(const std::function<void()>& callback)
    {
        constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

        constexpr ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus |
            ImGuiWindowFlags_MenuBar;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        {
            ImGui::PopStyleVar(3);

            // Submit the DockSpace
            BenzinAssert((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0);

            const ImGuiID dockspaceId = ImGui::GetID("BenzinDockSpace");
            ImGui::DockSpace(dockspaceId, ImVec2{ 0.0f, 0.0f }, dockspaceFlags);

            callback();
        }
        ImGui::End();
    }

    void ImGuiManager::SpawnImGuiManuBar()
    {
        ImGui::BeginMenuBar();
        {
            if (ImGui::BeginMenu("Tools"))
            {
                for (auto& tool : m_Tools)
                {
                    ImGui::MenuItem(tool->m_Name.data(), tool->m_Shortcut.data(), &tool->m_IsVisible);
                }

                ImGui::Separator();

                if (ImGui::MenuItem("ImGuiDemoWindow", "O", m_IsImGuiDemoWindowVisible))
                {
                    ToggleImGuiDemoWindow();
                }

                if (ImGui::MenuItem("UiSpawn", "F1", m_IsUiSpawnEnabled))
                {
                    ToggleUiSpawn();
                }

                ImGui::EndMenu();
            }

            for (const auto& spawnImGuiMenu : m_ImGuiSpawnMenuCallbacks)
            {
                spawnImGuiMenu();
            }
        }
        ImGui::EndMenuBar();
    }

    void ImGuiManager::ToggleImGuiDemoWindow()
    {
        m_IsImGuiDemoWindowVisible = !m_IsImGuiDemoWindowVisible;
    }

    void ImGuiManager::ToggleUiSpawn()
    {
        m_IsUiSpawnEnabled = !m_IsUiSpawnEnabled;
    }

    void ImGuiManager::SaveToolsVisiblity()
    {
        if (m_Tools.empty())
        {
            return;
        }

        for (const auto* tool : m_Tools)
        {
            m_IsToolVisibleMap[tool->m_Name.data()] = tool->m_IsVisible;
        }

        std::ofstream file{ g_ToolsVisiblityPath };
        for (const auto& [name, isVisible] : m_IsToolVisibleMap)
        {
            file << name << ' ' << isVisible << '\n';
        }
    }

    void ImGuiManager::LoadToolsVisiblity()
    {
        if (!std::filesystem::exists(g_ToolsVisiblityPath))
        {
            return;
        }

        BenzinAssert(m_IsToolVisibleMap.empty());

        std::ifstream file{ g_ToolsVisiblityPath };
        while (file.good())
        {
            std::string name;
            bool isVisible;

            file >> name;
            file >> isVisible;

            m_IsToolVisibleMap[name] = isVisible;
        }
    }

    // ImGuiPass

    ImGuiPass::ImGuiPass(ImGuiManager& imGuiManager, uint32_t imGuiTextureIndex)
        : m_ImGuiManager{ imGuiManager }
        , m_ImGuiTextureIndex{ imGuiTextureIndex }
    {}

    ImGuiPass::~ImGuiPass()
    {
        ms_Resources->DestroyTexture(m_ImGuiTextureIndex);
    }

    void ImGuiPass::OnWindowResize()
    {
        ms_Resources->CreateTexture(m_ImGuiTextureIndex, TextureCreation
        {
            .DebugName = "ImGuiTexture",
            .Format = (GraphicsFormat)CommandLineArgs::GetU32("BackBufferFormat"),
            .Width = GetWindowViewportWidth(),
            .Height = GetWindowViewportHeight(),
            .MipCount = 1,
            .AccessFlags = TextureAccessFlag::AllowRenderTarget,
        });
    }

    void ImGuiPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinGpuEvent(commandList, "ImGui");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "ImGui");

        const auto& imGuiTexture = ms_Textures->Get(m_ImGuiTextureIndex);

        commandList.SetViewport(ms_WindowViewport);
        commandList.SetScissorRect(ms_WindowScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            TransitionBarrier{ imGuiTexture, ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ imGuiTexture.GetRtv() });
        commandList.ClearRenderTarget(imGuiTexture);

        ImGui_ImplDX12_RenderDrawData(m_ImGuiManager.m_CurrentImGuiDrawData, commandList.GetD3D12GraphicsCommandList());
    }

}
