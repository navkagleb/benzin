#include "benzin/config/bootstrap.hpp"
#include "benzin/core/imgui_layer.hpp"

#include <third_party/imgui/imgui_internal.h>
#include <third_party/imgui/backends/imgui_impl_dx12.h>
#include <third_party/imgui/backends/imgui_impl_win32.h>

#include "benzin/core/asserter.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/gpu_timer.hpp"
#include "benzin/graphics/nvapi_wrapper.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/system/window.hpp"

namespace benzin
{

    ImGuiLayer::ImGuiLayer(const GraphicsRefs& graphicsRefs)
        : m_Window{ graphicsRefs.WindowRef }
        , m_Backend{ graphicsRefs.BackendRef }
        , m_Device{ graphicsRefs.DeviceRef }
        , m_SwapChain{ graphicsRefs.SwapChainRef }
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui::StyleColorsDark();

        m_FontDescriptor = m_Device.GetDescriptorManager().AllocateDescriptor(DescriptorType::Srv);

        BenzinAssert(ImGui_ImplWin32_Init(m_Window.GetWin64Window()));
        BenzinAssert(ImGui_ImplDX12_Init(
            m_Device.GetD3D12Device(),
            CommandLineArgs::GetFrameInFlightCount(),
            (DXGI_FORMAT)CommandLineArgs::GetBackBufferFormat(),
            m_Device.GetDescriptorManager().GetD3D12GpuResourceDescriptorHeap(),
            D3D12_CPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetCpuHandle() },
            D3D12_GPU_DESCRIPTOR_HANDLE{ m_FontDescriptor.GetGpuHandle() }
        ));
    }

    ImGuiLayer::~ImGuiLayer()
    {
        m_Device.GetDescriptorManager().FreeDescriptor(m_FontDescriptor);

        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::Begin()
    {
        ImGui_ImplDX12_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End(bool isNeedToClearBackBuffer)
    {
        auto& currentBackBuffer = m_SwapChain.GetCurrentBackBuffer();
        auto& commandList = m_Device.GetGraphicsCommandQueue().GetCommandList();

        BenzinPushGpuEvent(commandList, "ImGuiLayer");

        ImGui::Render();

        commandList.SetResourceBarrier(TransitionBarrier{ currentBackBuffer, ResourceState::RenderTarget });
        BenzinExecuteOnScopeExit([&]
        {
            commandList.SetResourceBarrier(TransitionBarrier{ currentBackBuffer, ResourceState::Common });
        });

        commandList.SetRenderTargets({ currentBackBuffer.GetRtv() });

        if (isNeedToClearBackBuffer)
        {
            commandList.ClearRenderTarget(currentBackBuffer.GetRtv());
        }

        ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList.GetD3D12GraphicsCommandList());
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        {
            // ImGui layer handle system events

            const auto& io = ImGui::GetIO();
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
            event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;
        }

        EventDispatcher dispatcher{ event };
        dispatcher.Dispatch(&ImGuiLayer::OnKeyPressed, *this);
    }

    void ImGuiLayer::OnImGuiRender()
    {
        if (m_IsDemoWindowEnabled)
        {
            ImGui::ShowDemoWindow(&m_IsDemoWindowEnabled);
        }

        ImGui::Begin("GPU Memory Info");
        {
            static constexpr auto vendorLibNames = std::to_array({ "ADL", "NvAPI" });

            if (ImGui::TreeNode("NvAPI CpuVisibleVram"))
            {
                const auto [totalSize, freeSize] = NvApiWrapper::GetCpuVisibleVramInBytes(m_Device.GetD3D12Device());
                ImGui::Text(BenzinFormatCstr("- TotalSize: {} b, {:.2f} mb, {:.2f} gb", totalSize, BytesToFloatMb(totalSize), BytesToFloatGb(totalSize)));
                ImGui::Text(BenzinFormatCstr("- FreeSize: {} b, {:.2f} mb, {:.2f} gb", freeSize, BytesToFloatMb(freeSize), BytesToFloatGb(freeSize)));

                ImGui::TreePop();
            }

            ImGui::Separator();
            for (const auto adapterIndex : std::views::iota(0u, m_Backend.GetAdapterCount()))
            {
                const AdapterInfo& adapterInfo = m_Backend.GetAdaptersInfo(adapterIndex);
                const AdapterMemoryInfo adapterMemoryInfo = m_Backend.GetAdapterMemoryInfo(adapterIndex);
                
                const uint64_t usedVram = adapterMemoryInfo.ProcessUsedDedicatedVramInBytes;
                const uint64_t availableVram = adapterInfo.TotalDedicatedVramInBytes - usedVram;
                const uint64_t availableVramRelativeToOsBudget = adapterMemoryInfo.DedicatedVramOsBudgetInBytes < usedVram ? 0 : adapterMemoryInfo.DedicatedVramOsBudgetInBytes - usedVram;

                ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_None;
                if (adapterIndex == m_Backend.GetMainAdapterIndex())
                {
                    nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;
                }

                if (ImGui::TreeNodeEx(&adapterInfo.Name, nodeFlags, adapterInfo.Name.c_str()))
                {
                    const auto nodeFlags = ImGuiTreeNodeFlags_DefaultOpen;

                    ImGui::Text(BenzinFormatCstr("- Total DedicatedVram: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterInfo.TotalDedicatedVramInBytes), BytesToFloatGb(adapterInfo.TotalDedicatedVramInBytes)));
                    ImGui::Text(BenzinFormatCstr("- Total DedicatedRam: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterInfo.TotalDedicatedRamInBytes), BytesToFloatGb(adapterInfo.TotalDedicatedRamInBytes)));
                    ImGui::Text(BenzinFormatCstr("- Total SharedRam: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterInfo.TotalSharedRamInBytes), BytesToFloatGb(adapterInfo.TotalSharedRamInBytes)));
                    ImGui::NewLine();

                    if (ImGui::TreeNodeEx("##treenode0", nodeFlags, "Dxgi Dedicated Vram"))
                    {   
                        ImGui::Text(BenzinFormatCstr("- OsBudget: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.DedicatedVramOsBudgetInBytes), BytesToFloatGb(adapterMemoryInfo.DedicatedVramOsBudgetInBytes)));
                        ImGui::Text(BenzinFormatCstr("- Used: {:.2f} mb, {:.2f} gb", BytesToFloatMb(usedVram), BytesToFloatGb(usedVram)));
                        ImGui::Text(BenzinFormatCstr("- Available: {:.2f} mb, {:.2f} gb", BytesToFloatMb(availableVram), BytesToFloatGb(availableVram)));
                        ImGui::Text(BenzinFormatCstr("- Available (relative to OS-budget): {:.2f} mb, {:.2f} gb", BytesToFloatMb(availableVramRelativeToOsBudget), BytesToFloatGb(availableVramRelativeToOsBudget)));
                        ImGui::NewLine();
                        
                        ImGui::TreePop();
                    }

                    if (ImGui::TreeNodeEx("##treenode1", nodeFlags, "Dxgi Shared Ram"))
                    {
                        ImGui::Text(BenzinFormatCstr("- OsBudget: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.SharedRamOsBudgetInBytes), BytesToFloatGb(adapterMemoryInfo.SharedRamOsBudgetInBytes)));
                        ImGui::Text(BenzinFormatCstr("- Used: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.ProcessUsedSharedRamInBytes), BytesToFloatGb(adapterMemoryInfo.ProcessUsedSharedRamInBytes)));
                        ImGui::NewLine();

                        ImGui::TreePop();
                    }

                    if (adapterInfo.IsOther())
                    {
                        continue;
                    }

                    const auto vendorLibName = vendorLibNames[magic_enum::enum_integer(adapterInfo.VendorType)];

                    if (ImGui::TreeNodeEx("##treenode2", nodeFlags, "%s Dedicated Vram", vendorLibName))
                    {
                        ImGui::Text(BenzinFormatCstr("- TotalUsed: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.TotalUsedDedicatedVramInBytes), BytesToFloatGb(adapterMemoryInfo.TotalUsedDedicatedVramInBytes)));
                        ImGui::Text(BenzinFormatCstr("- TotalAvailable: {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.AvailableDedicatedVramInBytes), BytesToFloatGb(adapterMemoryInfo.AvailableDedicatedVramInBytes)));
                        ImGui::Text(BenzinFormatCstr("- TotalAvailable (relative to OS-budget): {:.2f} mb, {:.2f} gb", BytesToFloatMb(adapterMemoryInfo.AvailableDedicatedVramRelativeToOsBudgetInBytes), BytesToFloatGb(adapterMemoryInfo.AvailableDedicatedVramRelativeToOsBudgetInBytes)));
                        ImGui::NewLine();

                        ImGui::TreePop();
                    }

                    ImGui::TreePop();
                }
            }
        }
        ImGui::End();

        // BottomPanel
        {
            static constexpr uint32_t rowCount = 2;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 10.0f, 5.0f });
            BenzinExecuteOnScopeExit([]{ ImGui::PopStyleVar(3); });

            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(184, 100, 0, 240));
            BenzinExecuteOnScopeExit([] { ImGui::PopStyleColor(); });

            const auto& context = *ImGui::GetCurrentContext();
            const float panelHeight =
                context.FontBaseSize * rowCount + // Base panel height
                context.Style.WindowPadding.y * 2.0f + // Add padding to top and bottom
                context.Style.ItemSpacing.y * (rowCount - 1); // Add spacing between rows

            ImGui::SetNextWindowPos(ImVec2{ 0.0f, context.IO.DisplaySize.y - panelHeight });
            ImGui::SetNextWindowSize(ImVec2{ context.IO.DisplaySize.x, panelHeight });

            const auto windowFlags = ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs;
            ImGui::Begin("Bottom Panel", nullptr, windowFlags);
            {
                const auto adapterMemoryInfo = m_Backend.GetMainAdapterMemoryInfo();

                ImGui::Text(BenzinFormatCstr(
                    "{} | "
                    "VRAM Local: {:.0f} / {:.0f} mb | "
                    "VRAM NonLocal: {:.0f} / {:.0f} mb | "
                    "CPU: {}, GPU: {}, ActiveFrame: {}",
                    m_Backend.GetMainAdapterInfo().Name,
                    BytesToFloatMb(adapterMemoryInfo.ProcessUsedDedicatedVramInBytes), BytesToFloatMb(adapterMemoryInfo.DedicatedVramOsBudgetInBytes),
                    BytesToFloatMb(adapterMemoryInfo.ProcessUsedSharedRamInBytes), BytesToFloatMb(adapterMemoryInfo.SharedRamOsBudgetInBytes),
                    m_Device.GetCpuFrameIndex(), m_Device.GetGpuFrameIndex(), m_Device.GetActiveFrameIndex()
                ));

                ImGui::Text(BenzinFormatCstr(
                    "({} x {}) | "
                    "FPS: {:.1f} ({:.3f} ms) | "
                    "Begin: {:.3f}, Process: {:.3f}, End: {:.3f}",
                    m_Window.GetWidth(), m_Window.GetHeight(),
                    m_FrameRate, m_FrameDeltaTimeMS,
                    ToFloatMs(m_ApplicationTimings[ApplicationTiming::BeginFrame]), ToFloatMs(m_ApplicationTimings[ApplicationTiming::ProcessFrame]), ToFloatMs(m_ApplicationTimings[ApplicationTiming::EndFrame])
                ));
            }
            ImGui::End();
        }
    }

    bool ImGuiLayer::OnKeyPressed(KeyPressedEvent& event)
    {
        switch (event.GetKeyCode())
        {
            case KeyCode::F1:
            {
                m_IsWidgetDrawingEnabled = !m_IsWidgetDrawingEnabled;
                break;
            }
            case KeyCode::O:
            {
                m_IsDemoWindowEnabled = !m_IsDemoWindowEnabled;
                break;
            }
        }

        return false;
    }

} // namespace benzin
