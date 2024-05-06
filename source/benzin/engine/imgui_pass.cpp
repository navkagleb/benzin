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
#include "benzin/system/event.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    // ImGuiTool

    ImGuiTool::ImGuiTool(std::string_view name, bool isVisible)
        : m_Name{ name }
        , m_IsVisible{ isVisible }
    {}
    
    void ImGuiTool::RenderImGuiWindow(const std::function<void()>& callback)
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
            CommandLineArgs::GetFrameInFlightCount(),
            (DXGI_FORMAT)CommandLineArgs::GetBackBufferFormat(),
            m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetCpuHandle() },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetGpuHandle() }
        ));
    }

    ImGuiManager::~ImGuiManager()
    {
        m_Device.GetDescriptorManager().FreeDescriptor(m_FontDescriptor);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiManager::OnEvent(Event& event)
    {
        // ImGuiManager handles system events
        const auto& io = ImGui::GetIO();
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;

        for (auto& imGuiTool : m_Tools)
        {
            imGuiTool->OnEvent(event);
        }
    }

    // ImGuiPass

    ImGuiPass::ImGuiPass(ImGuiManager& imGuiManager, uint32_t finalOutputTextureKey, uint32_t gpuTimingIndex)
        : m_ImGuiManager{ imGuiManager }
        , m_FinalOutputTextureKey{ finalOutputTextureKey }
        , m_GpuTimingIndex{ gpuTimingIndex }
    {}

    void ImGuiPass::OnRender() const
    {
        BenzinGrabTimeOnScopeExit(m_RenderTime);

        Begin();
        {
            ImGui::BeginMainMenuBar();
            {
                if (ImGui::BeginMenu("Tools"))
                {
                    for (auto& imGuiTool : m_ImGuiManager.m_Tools)
                    {
                        ImGui::MenuItem(imGuiTool->m_Name.data(), nullptr, &imGuiTool->m_IsVisible);
                    }

                    ImGui::EndMenu();
                }
            }
            ImGui::EndMainMenuBar();

            for (auto& imGuiTool : m_ImGuiManager.m_Tools)
            {
                if (imGuiTool->m_IsVisible)
                {
                    imGuiTool->OnImGuiRender();
                }
            }
        }
        End();
    }

    void ImGuiPass::Begin() const
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiPass::End() const
    {
        auto& gpuTimer = ms_Device->GetGpuTimer();
        BenzinGrabGpuTimeOnScopeExit(gpuTimer, m_GpuTimingIndex);

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "ImGuiPass");

        ImGui::Render();

        const auto& finalOutputTexture = *ms_RenderResources->GetTexture(m_FinalOutputTextureKey);

        BenzinMakeScopedResourceBarriers(
            commandList,
            TransitionBarrier{ finalOutputTexture, ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ finalOutputTexture.GetRtv() });

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetD3D12GraphicsCommandList());
    }

}
