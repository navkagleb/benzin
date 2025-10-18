#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/imgui_pass.hpp>

#include <benzin/core/buffer_writer.hpp>
#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/graphics/texture.hpp>  
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>
#include <benzin/system/key_event.hpp>
#include <benzin/system/window.hpp>

#include <backends/imgui_impl_dx12.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

BenzinAllowDereferenceOperatorForEnum(joint::ImGuiResources);
BenzinAllowDereferenceOperatorForEnum(joint::ImGuiSamplerIndex);

namespace benzin
{

    static const auto g_ToolVisiblityCacheFilePath = std::filesystem::absolute("bin/tools_visiblity.txt");

    static std::string_view GetToolDisplayName(std::string_view fullPath)
    {
        const size_t lastSlash = fullPath.find_last_of('/');

        if (lastSlash == std::string_view::npos)
            return fullPath;

        return fullPath.substr(lastSlash + 1);
    }

    // ImGuiTool

    ImGuiTool::ImGuiTool(std::string_view path, KeyCode shortcutKeyCode)
        : m_Path{ path }
        , m_ShortcutKeyCode{ shortcutKeyCode }
    {}

    void ImGuiTool::OnEvent(Event& event)
    {
        if (m_ShortcutKeyCode == KeyCode::Unknown)
            return;

        EventDispatcher dispatcher{ event };

        dispatcher.Dispatch<KeyPressedEvent>([this](const auto& event)
        {
            if (event.GetKeyCode() == m_ShortcutKeyCode)
            {
                m_IsVisible = !m_IsVisible; 
            }

            return false;
        });
    }

    void ImGuiTool::DrawWindow()
    {
        DrawWindow(ImGuiWindowFlags_None);
    }

    void ImGuiTool::DrawWindow(ImGuiWindowFlags flags)
    {
        if (!m_IsVisible)
            return;

        const std::string_view toolName = GetToolDisplayName(m_Path);
        if (ImGui::Begin(toolName.data(), &m_IsVisible, flags))
        {
            DrawWindowContent();
        }

        m_IsHovered = ImGui::IsWindowHovered();
        m_IsCollapsed = ImGui::IsWindowCollapsed();

        ImGui::End();

        if (!m_IsVisible)
        {
            m_IsHovered = false;
            m_IsCollapsed = false;
        }
    }

    // ImGuiManager

    ImGuiManager::ImGuiManager(Window& window, const TickTimer& frameTimer)
        : m_IntervalTimer{ frameTimer, std::chrono::milliseconds{ 1000 } }
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
        ImGuiTool::ms_IntervalTimer = &m_IntervalTimer;

        window.SetPreMessageHandlerCallback(ImGui_ImplWin32_WndProcHandler);

        LoadToolVisiblityCache();

        m_IntervalTimer.AddCallback([this](float, uint32_t)
        {
            SaveToolVisiblityCache();
        });
    }

    ImGuiManager::~ImGuiManager()
    {
        BenzinAssert(m_Tools.empty(), "Not all tools are unregistered!");

        SaveToolVisiblityCache();

        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiManager::BeginFrame()
    {
        m_IntervalTimer.AccumulateInterval();
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
                    ToggleUiDraw();
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

    void ImGuiManager::DrawUi()
    {
        BenzinProfile();

        if (m_IsUiDrawEnabled)
        {
            DrawDockSpace();
        }
    }

    void ImGuiManager::DrawDockSpace()
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

            DrawDockSpaceContent();
        }
        ImGui::End();
    }

    void ImGuiManager::DrawDockSpaceContent()
    {
        DrawManuBar();

        if (m_IsImGuiDemoWindowVisible)
        {
            ImGui::ShowDemoWindow(&m_IsImGuiDemoWindowVisible);
        }

        for (auto& tool : m_Tools)
        {
            m_ToolVisibilityCache[tool->m_UniqueId] = tool->m_IsVisible;

            tool->DrawWindow();
            tool->PostDrawWindow();
        };
    }

    void ImGuiManager::DrawManuBar()
    {
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("Benzin"))
        {
            if (ImGui::MenuItem("ImGuiDemoWindow", "O", m_IsImGuiDemoWindowVisible))
            {
                ToggleImGuiDemoWindow();
            }

            if (ImGui::MenuItem("UiDraw", "F1", m_IsUiDrawEnabled))
            {
                ToggleUiDraw();
            }

            ImGui::EndMenu();
        }

        for (auto& tool : m_Tools)
        {
            const std::vector<std::string_view> pathParts = SplitStringView(tool->m_Path, '/');
            DrawToolMenuPath(*tool, pathParts, 0);
        }

        ImGui::EndMenuBar();
    }

    void ImGuiManager::DrawToolMenuPath(ImGuiTool& tool, std::span<const std::string_view> pathParts, uint32_t depth)
    {
        if (pathParts.empty())
            return;

        // TODO: Hack for non null-terminated std::string_view
        constexpr size_t maxPartSize = 32;

        char partBuffer[maxPartSize];
        const size_t partSize = std::min(pathParts[depth].size(), maxPartSize - 1);

        std::memcpy(partBuffer, pathParts[depth].data(), partSize);
        partBuffer[partSize] = '\0';

        if (depth + 1 == pathParts.size())
        {
            const char* shortcutKeyName = tool.m_ShortcutKeyCode != KeyCode::Unknown ? magic_enum::enum_name(tool.m_ShortcutKeyCode).data() : nullptr;
            ImGui::MenuItem(partBuffer, shortcutKeyName, &tool.m_IsVisible);

            return;
        }

        if (ImGui::BeginMenu(partBuffer))
        {
            DrawToolMenuPath(tool, pathParts, depth + 1);
            ImGui::EndMenu();
        }
    }

    void ImGuiManager::ToggleImGuiDemoWindow()
    {
        m_IsImGuiDemoWindowVisible = !m_IsImGuiDemoWindowVisible;
    }

    void ImGuiManager::ToggleUiDraw()
    {
        m_IsUiDrawEnabled = !m_IsUiDrawEnabled;
    }

    void ImGuiManager::LoadToolVisiblityCache()
    {
        if (!std::filesystem::exists(g_ToolVisiblityCacheFilePath))
            return;

        BenzinAssert(m_ToolVisibilityCache.empty());

        std::ifstream file{ g_ToolVisiblityCacheFilePath };
        while (file.good())
        {
            uint64_t id;
            bool isVisible;

            file >> id;
            file >> isVisible;

            m_ToolVisibilityCache[id] = isVisible;
        }
    }

    void ImGuiManager::SaveToolVisiblityCache()
    {
        std::ofstream file{ g_ToolVisiblityCacheFilePath };
        for (const auto [id, isVisible] : m_ToolVisibilityCache)
        {
            file << id << ' ' << isVisible << '\n';
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
        ms_PsoManager->Create(PsoId::ImGui, [](VertexPsoProxy& proxy)
        {
            proxy.InputLayout.emplace_back("Position", GraphicsFormat::Rg32Float);
            proxy.InputLayout.emplace_back("Uv", GraphicsFormat::Rg32Float);
            proxy.InputLayout.emplace_back("Color", GraphicsFormat::Rgba8Unorm);

            BenzinAssert(GetFormatSizeInBytes(proxy.InputLayout[0].Format) == sizeof(ImDrawVert::pos));
            BenzinAssert(GetFormatSizeInBytes(proxy.InputLayout[1].Format) == sizeof(ImDrawVert::uv));
            BenzinAssert(GetFormatSizeInBytes(proxy.InputLayout[2].Format) == sizeof(ImDrawVert::col));

            proxy.Vs.FileName = "imgui_pass.hlsl";
            proxy.Ps.FileName = "imgui_pass.hlsl";

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
        const ImDrawData& imDrawData = *m_ImGuiManager.m_CurrentImGuiDrawData;

        RenderPass::m_IsRenderingEnabled = imDrawData.DisplaySize[0] != 0.0f && imDrawData.DisplaySize[1] != 0.0f;
        if (!m_IsRenderingEnabled)
            return;

        UpdateConsts(imDrawData);
        UpdateVertexAndIndexBuffers(imDrawData);
    }

    void ImGuiPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("ImGui");

        GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        D3D12_VIEWPORT d3d12Viewport = {};
        d3d12Viewport.Width = (float)ms_WindowWidth;
        d3d12Viewport.Height = (float)ms_WindowHeight;
        d3d12Viewport.MinDepth = 0.0f;
        d3d12Viewport.MaxDepth = 1.0f;

        D3D12_RECT d3d12ScissorRect = {};
        d3d12ScissorRect.right = ms_WindowWidth;
        d3d12ScissorRect.bottom = ms_WindowHeight;

        cmdList.GetD3D12GraphicsCommandList()->RSSetViewports(1, &d3d12Viewport);
        cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &d3d12ScissorRect);

        cmdList.SetPrimitiveTopology(PrimitiveTopology::TriangleList);
        cmdList.SetVertexPso(ms_PsoManager->GetVertex(PsoId::ImGui));
        cmdList.SetGraphicsCbv(UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));
        cmdList.SetBlendFactor({});

        auto& [vertexBuffer, indexBuffer] = m_FrameContexts[ms_Device->GetActiveFrameIndex()];
        cmdList.SetVertexBuffer(*vertexBuffer);
        cmdList.SetIndexBuffer(*indexBuffer);

        const Texture& backBuffer = ms_SwapChain->GetCurrentBackBuffer();

        BenzinScopedResourceBarriers(
            cmdList,
            TransitionBarrier{ backBuffer, ResourceState::RenderTarget });

        cmdList.SetRenderTargets({ backBuffer.GetRtv() });
        cmdList.ClearRenderTarget(backBuffer, DirectX::XMFLOAT4{});

        RenderImDrawData(cmdList);
    }

    void ImGuiPass::UploadFontTexture()
    {
        ImGuiIO& io = ImGui::GetIO();

        unsigned char* pixels;
        int width;
        int height;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

        MakeUniquePtr(m_FontTexture, *ms_Device, TextureCreation
        {
            .DebugName = "ImGui_Font",
            .Format = GraphicsFormat::Rgba8Unorm,
            .Width = (uint32_t)width,
            .Height = (uint32_t)height,
            .MipCount = 1,
        });

        const uint32_t textureSizeInBytes = width * height * GetFormatSizeInBytes(m_FontTexture->GetFormat());
        BenzinAssert(textureSizeInBytes == m_FontTexture->GetSizeInBytes());

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList(m_FontTexture->GetSizeInBytes());
        cmdList.UploadToTexture(*m_FontTexture, ToSpan((const std::byte*)pixels, textureSizeInBytes));
    }

    void ImGuiPass::UpdateConsts(const ImDrawData& imDrawData)
    {
        const float left = imDrawData.DisplayPos.x;
        const float right = imDrawData.DisplayPos.x + imDrawData.DisplaySize.x;
        const float top = imDrawData.DisplayPos.y;
        const float bottom = imDrawData.DisplayPos.y + imDrawData.DisplaySize.y;
        const float nearZ = 1.0f;
        const float farZ = -1.0f;

        m_Consts.m_ViewToClipOrtho = DirectX::XMMatrixOrthographicOffCenterRH(left, right, bottom, top, nearZ, farZ);
    }

    void ImGuiPass::UpdateVertexAndIndexBuffers(const ImDrawData& imDrawData)
    {
        BenzinProfile();

        auto& [vertexBuffer, indexBuffer] = m_FrameContexts[ms_Device->GetActiveFrameIndex()];

        if (vertexBuffer.get() == nullptr || (int)vertexBuffer->GetElementCount() < imDrawData.TotalVtxCount)
        {
            MakeUniquePtr(vertexBuffer, *ms_Device, BufferCreation
            {
                .DebugName = "ImGuiPass::VertexBuffer",
                .HeapType = GpuHeapType::GpuUpload,
                .Type = BufferType::Structured,
                .ElementSizeInBytes = sizeof(ImDrawVert),
                .ElementCount = (uint32_t)imDrawData.TotalVtxCount + 5000, // TODO: 5000 magic number
            });
        }

        if (indexBuffer.get() == nullptr || (int)indexBuffer->GetElementCount() < imDrawData.TotalIdxCount)
        {
            BenzinAssert(GetFormatSizeInBytes(GraphicsFormat::R16Uint) == sizeof(ImDrawIdx));

            MakeUniquePtr(indexBuffer, *ms_Device, BufferCreation
            {
                .DebugName = "ImGuiPass::IndexBuffer",
                .HeapType = GpuHeapType::GpuUpload,
                .Type = BufferType::Format,
                .Format = GraphicsFormat::R16Uint,
                .ElementSizeInBytes = sizeof(ImDrawIdx),
                .ElementCount = (uint32_t)imDrawData.TotalIdxCount + 10000, // TODO: 10000 magic number
            });
        }

        BufferWriter vertexWriter = MakeBufferWriter(*vertexBuffer);
        BufferWriter indexWriter = MakeBufferWriter(*indexBuffer);

        const auto imDrawLists = ToSpan(imDrawData.CmdLists.begin(), imDrawData.CmdListsCount);
        for (const ImDrawList* imDrawList : imDrawLists)
        {
            vertexWriter.WriteArray(ToSpan(imDrawList->VtxBuffer.Data, imDrawList->VtxBuffer.Size));
            indexWriter.WriteArray(ToSpan(imDrawList->IdxBuffer.Data, imDrawList->IdxBuffer.Size));
        }
    }

    void ImGuiPass::RenderImDrawData(GraphicsCmdList& cmdList) const
    {
        BenzinProfile();

        const ImDrawData& imDrawData = *m_ImGuiManager.m_CurrentImGuiDrawData;
        const ImVec2 clipOff = imDrawData.DisplayPos;

        int globalVertexOffset = 0;
        int globalIndexOffset = 0;
        for (int cmdListIndex = 0; cmdListIndex < imDrawData.CmdListsCount; ++cmdListIndex)
        {
            const ImDrawList* imCmdList = imDrawData.CmdLists[cmdListIndex];
            BenzinAssert(imCmdList != nullptr);

            for (const ImDrawCmd& imDrawCmd : imCmdList->CmdBuffer)
            {
                const DirectX::XMINT2 clipMin{ (int32_t)(imDrawCmd.ClipRect.x - clipOff.x), (int32_t)(imDrawCmd.ClipRect.y - clipOff.y) };
                const DirectX::XMINT2 clipMax{ (int32_t)(imDrawCmd.ClipRect.z - clipOff.x), (int32_t)(imDrawCmd.ClipRect.w - clipOff.y) };

                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    continue;

                D3D12_RECT d3d12ScissorRect = {};
                d3d12ScissorRect.left = clipMin.x;
                d3d12ScissorRect.top = clipMin.y;
                d3d12ScissorRect.right = clipMax.x;
                d3d12ScissorRect.bottom = clipMax.y;
                cmdList.GetD3D12GraphicsCommandList()->RSSetScissorRects(1, &d3d12ScissorRect);

                uint32_t srvGpuHeapIndex;
                joint::ImGuiSamplerIndex samplerIndex;
                GetImGuiResources(imDrawCmd, srvGpuHeapIndex, samplerIndex);

                cmdList.SetGraphicsRootConstant(*joint::ImGuiResources::Texture, srvGpuHeapIndex);
                cmdList.SetGraphicsRootConstant(*joint::ImGuiResources::SamplerIndex, *samplerIndex);

                cmdList.DrawIndexed(imDrawCmd.ElemCount, imDrawCmd.IdxOffset + globalIndexOffset, imDrawCmd.VtxOffset + globalVertexOffset);
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
