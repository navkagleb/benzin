#include "benzin/config/bootstrap.hpp"
#include "benzin/engine/imgui_pass.hpp"

#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    // ImGuiTool

    ImGuiTool::ImGuiTool(std::string_view name, bool isVisible)
        : m_Name{ name }
        , m_IsVisible{ isVisible }
    {}
    
    void ImGuiTool::SpawnImGuiWindow(const std::function<void()>& callback)
    {
        ImGui::Begin(m_Name.data(), &m_IsVisible);
        {
            callback();
        }
        ImGui::End();
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

        ImGui::StyleColorsDark();

        m_FontDescriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv);

        BenzinEnsure(ImGui_ImplWin32_Init(window.GetWin64Window()));
        BenzinEnsure(ImGui_ImplDX12_Init(
            m_Device.GetD3D12Device(),
            CommandLineArgs::g_FrameInFlightCount,
            (DXGI_FORMAT)CommandLineArgs::g_BackBufferFormat,
            m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetCpuHandle() },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetGpuHandle() }
        ));
    }

    ImGuiManager::~ImGuiManager()
    {
        for (auto* tool : m_Tools)
        {
            delete tool;
        }
        m_Tools.clear();

        m_Device.GetDescriptorManager().FreeDescriptor(m_FontDescriptor);

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
        ImGui::Render();
        m_CurrentImGuiDrawData = ImGui::GetDrawData();
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
                    m_IsDemoWindowVisible = !m_IsDemoWindowVisible;
                    break;
                }
                case KeyCode::F1:
                {
                    m_IsSpawnEnabled = !m_IsSpawnEnabled;
                    break;
                }
            }

            return false;
        });

        // ImGuiManager handles system events
        const auto& io = ImGui::GetIO();
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;

        for (auto& tool : m_Tools)
        {
            tool->OnEvent(event);
        }
    }

    void ImGuiManager::SpawnUi()
    {
        if (!m_IsSpawnEnabled)
        {
            return;
        }

        SpawnImGuiDockSpace([this]
        {
            SpawnImGuiManuBar();

            if (m_IsDemoWindowVisible)
            {
                ImGui::ShowDemoWindow(&m_IsDemoWindowVisible);
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

    void ImGuiManager::SpawnImGuiDockSpace(const std::function<void()>& callback)
    {
        static constexpr ImGuiDockNodeFlags dockspaceFlags = ImGuiDockNodeFlags_None;

        static constexpr ImGuiWindowFlags windowFlags =
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
            ImGuiIO& io = ImGui::GetIO();
            if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
            {
                const ImGuiID dockspaceId = ImGui::GetID("BenzinDockSpace");
                ImGui::DockSpace(dockspaceId, ImVec2{ 0.0f, 0.0f }, dockspaceFlags);
            }

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
                    ImGui::MenuItem(tool->m_Name.data(), nullptr, &tool->m_IsVisible);
                }

                ImGui::EndMenu();
            }
        }
        ImGui::EndMenuBar();
    }

    // ImGuiPass

    ImGuiPass::ImGuiPass(ImGuiManager& imGuiManager, uint32_t imGuiTextureKey, uint32_t gpuTimingIndex)
        : m_ImGuiManager{ imGuiManager }
        , m_ImGuiTextureKey{ imGuiTextureKey }
        , m_GpuTimingIndex{ gpuTimingIndex }
    {}

    void ImGuiPass::OnWindowResize(uint32_t width, uint32_t height)
    {
        benzin::MakeUniquePtr(ms_Resources->GetTexture(m_ImGuiTextureKey), *ms_Device, benzin::TextureCreation
        {
            .DebugName = "ImGuiTexture",
            .Format = benzin::CommandLineArgs::g_BackBufferFormat,
            .Width = width,
            .Height = height,
            .MipCount = 1,
            .Flags = benzin::TextureFlag::AllowRenderTarget,
        });
    }

    void ImGuiPass::OnRender() const
    {
        BenzinGrabTimeOnScopeExit(m_CpuRenderTime);

        auto& gpuTimer = ms_Device->GetGpuTimer();
        BenzinGrabGpuTimeOnScopeExit(gpuTimer, m_GpuTimingIndex);

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "ImGuiPass");

        const auto& imGuiTexture = *ms_Resources->GetTexture(m_ImGuiTextureKey);

        commandList.SetViewport(ms_WindowViewport);
        commandList.SetScissorRect(ms_WindowScissorRect);

        BenzinMakeScopedResourceBarriers(
            commandList,
            TransitionBarrier{ imGuiTexture, ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ imGuiTexture.GetRtv() });
        commandList.ClearRenderTarget(imGuiTexture.GetRtv());

        ImGui_ImplDX12_RenderDrawData(m_ImGuiManager.m_CurrentImGuiDrawData, commandList.GetD3D12GraphicsCommandList());
    }

}
