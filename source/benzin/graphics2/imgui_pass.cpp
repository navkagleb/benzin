#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/imgui_pass.hpp"

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

#include <shaders/joint/imgui_resources.hpp>

#include "benzin/core/buffer_writer.hpp"
#include "benzin/core/command_line_args.hpp"
#include "benzin/core/profiler.hpp"
#include "benzin/graphics/buffer.hpp"
#include "benzin/graphics/command_list.hpp"
#include "benzin/graphics/command_queue.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/pso.hpp"
#include "benzin/graphics/swap_chain.hpp"
#include "benzin/graphics/texture.hpp"
#include "benzin/graphics/unified_root_signature.hpp"
#include "benzin/graphics2/const_buffer_pool.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/graphics2/gpu_profiler.hpp"
#include "benzin/graphics2/pso_manager.hpp"
#include "benzin/system/key_event.hpp"
#include "benzin/system/window.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

BenzinEnableUnaryPlusForEnum(joint::ImGuiResources);
BenzinEnableUnaryPlusForEnum(joint::ImGuiSamplerIndex);

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

    ImGuiManager::ImGuiManager(Window& window, Device& device, const TickTimer& frameTimer)
        : m_Device{ device }
    {
        IMGUI_CHECKVERSION();

        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        ImGui::StyleColorsClassic();

        BenzinEnsure(ImGui_ImplWin32_Init(window.GetWin64Window()));

        ImGuiTool::ms_Window = &window;
        ImGuiTool::ms_FrameTimer = &frameTimer;

        window.SetPreMessageHandlerCallback(ImGui_ImplWin32_WndProcHandler);

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

        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiManager::BeginUiFrame() const
    {
        BenzinProfile();

        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        m_CurrentImGuiDrawData = nullptr;
    }

    void ImGuiManager::EndUiFrame() const
    {
        BenzinProfile();

        ImGui::EndFrame();
        ImGui::Render();

        m_CurrentImGuiDrawData = ImGui::GetDrawData();

        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    void ImGuiManager::OnEvent(Event& event)
    {
        BenzinProfile();

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

        for (auto& tool : m_Tools)
        {
            tool->OnEvent(event);
        }

        // ImGuiManager handles system events
        const ImGuiIO& io = ImGui::GetIO();
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Keyboard) & io.WantCaptureKeyboard;
        event.m_IsHandled |= event.IsInCategory(EventCategoryFlag::Mouse) & io.WantCaptureMouse;
    }

    void ImGuiManager::SpawnUi()
    {
        BenzinProfile();

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
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

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

        ImGui::Begin("DockSpace", nullptr, windowFlags);
        {
            ImGui::PopStyleVar(3);

            // Submit the DockSpace
            BenzinAssert((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable) != 0);

            const ImGuiID dockspaceId = ImGui::GetID("BenzinDockSpace");
            ImGui::DockSpace(dockspaceId, ImVec2{ 0.0f, 0.0f }, ImGuiDockNodeFlags_None);

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

    uint64_t ImGuiPass::PackImTextureId(const Descriptor& viewDescriptor, joint::ImGuiSamplerIndex samplerIndex)
    {
        static_assert(sizeof(ImTextureID) == sizeof(uint64_t));

        uint64_t packedData = 0;
        packedData |= ((uint64_t)viewDescriptor.GetGpuHeapIndex()) << 32;
        packedData |= (uint32_t)samplerIndex;

        return packedData;
    }

    static void UnpackImTextureId(ImTextureID imTextureId, uint32_t& outTextureSrvHeapIndex, joint::ImGuiSamplerIndex& outSamplerIndex)
    {
        outTextureSrvHeapIndex = (uint32_t)(imTextureId >> 32);
        outSamplerIndex = (joint::ImGuiSamplerIndex)(imTextureId & std::numeric_limits<uint32_t>::max());

        BenzinAssert(magic_enum::enum_contains(outSamplerIndex));
    }

    ImGuiPass::ImGuiPass(ImGuiManager& imGuiManager)
        : m_ImGuiManager{ imGuiManager }
    {
        m_FrameContexts.resize(CommandLineArgs::GetU32("FrameInFlightCount"));

        ms_PsoManager->Create(PsoId::ImGui, [](GraphicsPsoProxy& proxy)
        {
            proxy.DebugName = "ImGui";
            proxy.InputLayout.emplace_back("Position", GraphicsFormat::Rg32Float);
            proxy.InputLayout.emplace_back("Uv", GraphicsFormat::Rg32Float);
            proxy.InputLayout.emplace_back("Color", GraphicsFormat::Rgba8Unorm);
            proxy.VsFileName = "imgui_pass.hlsl";
            proxy.PsFileName = "imgui_pass.hlsl";

            proxy.PrimitiveTopologyType = PrimitiveTopologyType::Triangle;

            proxy.RasterizerState.CullMode = CullMode::None;

            proxy.DepthState.IsEnabled = false;
            proxy.DepthState.IsWriteEnabled = false;
            proxy.StencilState.IsEnabled = false;

            proxy.RenderTargetFormats.emplace_back(GraphicsFormat::Rgba8Unorm);

            proxy.BlendState.IsAlphaToCoverageStateEnabled = false;
            proxy.BlendState.RenderTargetStates.push_back(BlendState::RenderTargetState
            {
                .IsEnabled = true,
                .ColorEquation
                {
                    .SourceFactor = BlendColorFactor::SourceAlpha,
                    .DestinationFactor = BlendColorFactor::InverseSourceAlpha,
                    .Operation = BlendOperation::Add,
                },
                .AlphaEquation
                {
                    .SourceFactor = BlendAlphaFactor::SourceAlpha,
                    .DestinationFactor = BlendAlphaFactor::InverseSourceAlpha,
                    .Operation = BlendOperation::Add,
                },
            });
        });

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
    }

    ImGuiPass::~ImGuiPass()
    {
        ms_PsoManager->Destroy(PsoId::ImGui);
    }

    void ImGuiPass::OnZeroFrameInit()
    {
        UploadFontTexture();
    }

    void ImGuiPass::OnUpdate()
    {
        const ImDrawData& imDrawData = m_ImGuiManager.GetImDrawData();

        UpdateConsts(imDrawData);
        UpdateVertexAndIndexBuffers(imDrawData);
    }

    void ImGuiPass::OnRender() const
    {
        BenzinProfile();

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "ImGui");

        commandList.SetViewport(ms_WindowViewport);
        commandList.SetPrimitiveTopology(PrimitiveTopology::TriangleList);
        commandList.SetGraphicsPso(ms_PsoManager->GetGraphics(PsoId::ImGui));
        commandList.SetGraphicsCbv(UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));
        commandList.SetBlendFactor({});

        auto& [vertexBuffer, indexBuffer] = m_FrameContexts[ms_Device->GetActiveFrameIndex()];
        commandList.SetVertexBuffer(*vertexBuffer);
        commandList.SetIndexBuffer(*indexBuffer);

        const auto& imGuiTexture = ms_SwapChain->GetCurrentBackBuffer();

        BenzinMakeScopedResourceBarriers(
            commandList,
            TransitionBarrier{ imGuiTexture, ResourceState::RenderTarget },
        );

        commandList.SetRenderTargets({ imGuiTexture.GetRtv() });
        commandList.ClearRenderTarget(imGuiTexture, DirectX::XMFLOAT4{});

        RenderImDrawData(commandList);
    }

    void ImGuiPass::UploadFontTexture()
    {
        ImGuiIO& io = ImGui::GetIO();

        unsigned char* pixels;
        int width, height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        MakeUniquePtr(m_FontTexture, *ms_Device, TextureCreation
        {
            .DebugName = "ImGui_Font",
            .Format = GraphicsFormat::Rgba8Unorm,
            .Width = (uint32_t)width,
            .Height = (uint32_t)height,
            .MipCount = 1,
        });

        const uint32_t textureSize = width * height * 4;
        BenzinAssert(textureSize == m_FontTexture->GetSize());

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(m_FontTexture->GetSize());
        commandList.UploadToTextureTopMip(*m_FontTexture, std::as_bytes(ToSpan(pixels, textureSize)));
    }

    void ImGuiPass::UpdateConsts(const ImDrawData& imDrawData)
    {
        const float left = imDrawData.DisplayPos.x;
        const float right = imDrawData.DisplayPos.x + imDrawData.DisplaySize.x;
        const float top = imDrawData.DisplayPos.y;
        const float bottom = imDrawData.DisplayPos.y + imDrawData.DisplaySize.y;
        const float nearZ = 1.0f;
        const float farZ = -1.0f;

        m_Consts.ViewToClipOrtho = DirectX::XMMatrixOrthographicOffCenterRH(left, right, bottom, top, nearZ, farZ);
    }

    void ImGuiPass::UpdateVertexAndIndexBuffers(const ImDrawData& imDrawData)
    {
        BenzinProfile();

        auto& [vertexBuffer, indexBuffer] = m_FrameContexts[ms_Device->GetActiveFrameIndex()];

        if (vertexBuffer.get() == nullptr || (int)vertexBuffer->GetElementCount() < imDrawData.TotalVtxCount)
        {
            MakeUniquePtr(vertexBuffer, *ms_Device, BufferCreation
            {
                .DebugName = "ImGui_VertexBuffer",
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Vertex,
                .ElementSize = sizeof(ImDrawVert),
                .ElementCount = (uint32_t)imDrawData.TotalVtxCount + 5000, // TODO: 5000 magic number
            });
        }

        if (indexBuffer.get() == nullptr || (int)indexBuffer->GetElementCount() < imDrawData.TotalIdxCount)
        {
            BenzinAssert(sizeof(ImDrawIdx) == GetFormatSize(GraphicsFormat::R16Uint));

            MakeUniquePtr(indexBuffer, *ms_Device, BufferCreation
            {
                .DebugName = "ImGui_IndexBuffer",
                .MemoryType = ResourceMemoryType::Upload,
                .Type = BufferType::Index,
                .Format = GraphicsFormat::R16Uint,
                .ElementSize = sizeof(ImDrawIdx),
                .ElementCount = (uint32_t)imDrawData.TotalIdxCount + 10000, // TODO: 10000 magic number
            });
        }

        BufferWriter vertexWriter{ vertexBuffer->GetCpuMappedData(), vertexBuffer->GetSize() };
        BufferWriter indexWriter{ indexBuffer->GetCpuMappedData(), indexBuffer->GetSize() };
        for (int cmdListIndex = 0; cmdListIndex < imDrawData.CmdListsCount; cmdListIndex++)
        {
            const ImDrawList* cmdList = imDrawData.CmdLists[cmdListIndex];

            vertexWriter.WriteArray(ToSpan(cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size));
            indexWriter.WriteArray(ToSpan(cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size));
        }
    }

    void ImGuiPass::RenderImDrawData(GraphicsCommandList& commandList) const
    {
        BenzinProfile();

        const ImDrawData& imDrawData = m_ImGuiManager.GetImDrawData();
        const ImVec2 clipOff = imDrawData.DisplayPos;

        int globalVertexOffset = 0;
        int globalIndexOffset = 0;
        for (int cmdListIndex = 0; cmdListIndex < imDrawData.CmdListsCount; cmdListIndex++)
        {
            const ImDrawList* imCmdList = imDrawData.CmdLists[cmdListIndex];
            BenzinAssert(imCmdList != nullptr);

            for (const ImDrawCmd& imDrawCmd : imCmdList->CmdBuffer)
            {
                const ImVec2 clipMin{ imDrawCmd.ClipRect.x - clipOff.x, imDrawCmd.ClipRect.y - clipOff.y };
                const ImVec2 clipMax{ imDrawCmd.ClipRect.z - clipOff.x, imDrawCmd.ClipRect.w - clipOff.y };

                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                commandList.SetScissorRect(ScissorRect
                {
                    .X = clipMin.x,
                    .Y = clipMin.y,
                    .Width = clipMax.x - clipMin.x,
                    .Height = clipMax.y - clipMin.y,
                });

                uint32_t srvGpuHeapIndex;
                joint::ImGuiSamplerIndex samplerIndex;
                GetImGuiResources(imDrawCmd, srvGpuHeapIndex, samplerIndex);

                commandList.SetGraphicsRootConstant(+joint::ImGuiResources::Texture, srvGpuHeapIndex);
                commandList.SetGraphicsRootConstant(+joint::ImGuiResources::SamplerIndex, +samplerIndex);

                commandList.DrawIndexed(imDrawCmd.ElemCount, imDrawCmd.IdxOffset + globalIndexOffset, imDrawCmd.VtxOffset + globalVertexOffset);
            }

            globalVertexOffset += imCmdList->VtxBuffer.Size;
            globalIndexOffset += imCmdList->IdxBuffer.Size;
        }
    }

    void ImGuiPass::GetImGuiResources(const ImDrawCmd& imDrawCmd, uint32_t& outTextureSrvHeapIndex, joint::ImGuiSamplerIndex& outSamplerIndex) const
    {
        if (imDrawCmd.TextureId != 0)
        {
            UnpackImTextureId(imDrawCmd.TextureId, outTextureSrvHeapIndex, outSamplerIndex);
            return;
        }

        outTextureSrvHeapIndex = m_FontTexture->GetSrv().GetGpuHeapIndex();
        outSamplerIndex = joint::ImGuiSamplerIndex::Linear;
    }

}
