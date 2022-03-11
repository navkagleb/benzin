#include "application.h"

#include <fstream>
#include <numbers>

#include <imgui/imgui.h>
#include <imgui/backends/imgui_impl_dx12.h>
#include <imgui/backends/imgui_impl_win32.h>

#include "event_dispatcher.h"
#include "window_event.h"
#include "key_event.h"

namespace Spieler
{

    bool Application::Init(const std::string& title, std::uint32_t width, std::uint32_t height)
    {
#if defined(SPIELER_DEBUG)
        _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

        SPIELER_RETURN_IF_FAILED(m_Window.Init(title, width, height));

        m_Window.SetEventCallbackFunction([&](Event& event) { WindowEventCallback(event); });

        SPIELER_RETURN_IF_FAILED(m_Renderer.Init(m_Window));

        m_Renderer.ResetCommandList();

        SPIELER_RETURN_IF_FAILED(m_CBVDescriptorHeap.Init(DescriptorHeapType_CBV, 2));
        SPIELER_RETURN_IF_FAILED(m_ImGuiDescriptorHeap.Init(DescriptorHeapType_CBV, 1));
        SPIELER_RETURN_IF_FAILED(InitMSAAQualitySupport());

        InitViewport();
        InitScissorRect();

#if SPIELER_USE_IMGUI
        SPIELER_RETURN_IF_FAILED(InitImGui());
#endif

        BoxGeometryProps boxProps;
        boxProps.Width              = 30.0f;
        boxProps.Height             = 20.0f;
        boxProps.Depth              = 25.0f;
        boxProps.SubdivisionCount   = 2;

        GridGeometryProps gridProps;
        gridProps.Width         = 100.0f;
        gridProps.Depth         = 100.0f;
        gridProps.RowCount      = 10;
        gridProps.ColumnCount   = 10;

        CylinderGeometryProps cylinderProps;
        cylinderProps.TopRadius     = 0;
        cylinderProps.BottomRadius  = 20.0f;
        cylinderProps.Height        = std::sqrt(200.0f) * 2;
        cylinderProps.SliceCount    = 40;
        cylinderProps.StackCount    = 1;

        SphereGeometryProps sphereProps;
        sphereProps.Radius      = 20.0f;
        sphereProps.SliceCount  = 30;
        sphereProps.StackCount  = 10;

        GeosphereGeometryProps geosphereProps;
        geosphereProps.Radius           = 20.0f;
        geosphereProps.SubdivisionCount = 1;

        const MeshData meshData = GeometryGenerator::GenerateGrid<BasicVertex, std::uint32_t>(gridProps);

        SPIELER_RETURN_IF_FAILED(m_VertexBuffer.Init(meshData.Vertices.data(), meshData.Vertices.size()));

        m_VertexBuffer.SetName(L"Cylinder Vertex Buffer");

        SPIELER_RETURN_IF_FAILED(m_IndexBuffer.Init(meshData.Indices.data(), meshData.Indices.size()));

        m_IndexCount = meshData.Indices.size();

        SPIELER_RETURN_IF_FAILED(m_ConstantBuffer.Init(m_CBVDescriptorHeap.GetCPUHandle(0)));
        SPIELER_RETURN_IF_FAILED(m_RootSignature.Init());

        InitViewMatrix();
        InitProjectionMatrix();

        SPIELER_RETURN_IF_FAILED(InitPipelineState());

        m_Renderer.ExexuteCommandList();

        return true;
    }

    int Application::Run()
    {
        MSG message{};

        m_Timer.Reset();

        m_IsRunning = true;

        while (m_IsRunning && message.message != WM_QUIT)
        {
            if (::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                ::TranslateMessage(&message);
                ::DispatchMessage(&message);
            }
            else
            {
                if (m_IsPaused)
                {
                    ::Sleep(100);
                }
                else
                {
                    m_Timer.Tick();

                    const float dt = m_Timer.GetDeltaTime();

#if SPIELER_USE_IMGUI
                    // Start the Dear ImGui frame
                    ImGui_ImplDX12_NewFrame();
                    ImGui_ImplWin32_NewFrame();
                    ImGui::NewFrame();
#endif

                    CalcStats();
                    OnUpdate(dt);

#if SPIELER_USE_IMGUI
                    OnImGuiRender(dt);

                    // Rendering
                    ImGui::Render();
#endif

                    if (!OnRender(dt))
                    {
                        break;
                    }
                }
            }
        }

        return static_cast<int>(message.lParam);
    }

    bool Application::InitMSAAQualitySupport()
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS qualityLevels{};
        qualityLevels.Format            = m_Renderer.GetSwapChainProps().BufferFormat;
        qualityLevels.SampleCount       = 4;
        qualityLevels.Flags             = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        qualityLevels.NumQualityLevels  = 0;

        SPIELER_RETURN_IF_FAILED(m_Renderer.m_Device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &qualityLevels, sizeof(qualityLevels)));

        m_4xMSAAQuality = qualityLevels.NumQualityLevels;
        SPIELER_ASSERT(m_4xMSAAQuality > 0);

        return true;
    }

    bool Application::InitImGui()
    {
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();

        // Setup Platform/Renderer backends
        ImGui_ImplWin32_Init(m_Window.GetHandle());
        ImGui_ImplDX12_Init(
            m_Renderer.m_Device.Get(),
            1,
            m_Renderer.GetSwapChainProps().BufferFormat, 
            reinterpret_cast<ID3D12DescriptorHeap*>(&m_CBVDescriptorHeap),
            m_ImGuiDescriptorHeap.GetCPUHandle(0),
            m_ImGuiDescriptorHeap.GetGPUHandle(0)
        );

        return true;
    }

    bool Application::InitPipelineState()
    {
        // Vertex shader
        SPIELER_RETURN_IF_FAILED(m_VertexShader.LoadFromFile(L"assets/shaders/basic_vertex.fx", "VS_Main"));

        D3D12_SHADER_BYTECODE vsDesc{};
        vsDesc.pShaderBytecode  = m_VertexShader.GetData();
        vsDesc.BytecodeLength   = m_VertexShader.GetSize();

        // Pixel shader
        SPIELER_RETURN_IF_FAILED(m_PixelShader.LoadFromFile(L"assets/shaders/basic_vertex.fx", "PS_Main"));

        D3D12_SHADER_BYTECODE psDesc{};
        psDesc.pShaderBytecode  = m_PixelShader.GetData();
        psDesc.BytecodeLength   = m_PixelShader.GetSize();

        // Stream output
        D3D12_STREAM_OUTPUT_DESC streamOutputDesc{};
        streamOutputDesc.pSODeclaration     = nullptr;
        streamOutputDesc.NumEntries         = 0;
        streamOutputDesc.pBufferStrides     = nullptr;
        streamOutputDesc.NumStrides         = 0;
        streamOutputDesc.RasterizedStream   = 0;

        // Blend state
        D3D12_BLEND_DESC blendDesc{};
        blendDesc.AlphaToCoverageEnable                 = false;
        blendDesc.IndependentBlendEnable                = false;
        blendDesc.RenderTarget[0].BlendEnable           = false;
        blendDesc.RenderTarget[0].LogicOpEnable         = false;
        blendDesc.RenderTarget[0].SrcBlend              = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlend             = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOp               = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].SrcBlendAlpha         = D3D12_BLEND_ONE;
        blendDesc.RenderTarget[0].DestBlendAlpha        = D3D12_BLEND_ZERO;
        blendDesc.RenderTarget[0].BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        blendDesc.RenderTarget[0].LogicOp               = D3D12_LOGIC_OP_NOOP;
        blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Rasterizer state
        D3D12_RASTERIZER_DESC rasterizerDesc{};
        rasterizerDesc.FillMode               = D3D12_FILL_MODE_WIREFRAME;
        rasterizerDesc.CullMode               = D3D12_CULL_MODE_NONE;
        rasterizerDesc.FrontCounterClockwise  = false;
        rasterizerDesc.DepthBias              = D3D12_DEFAULT_DEPTH_BIAS;
        rasterizerDesc.DepthBiasClamp         = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        rasterizerDesc.SlopeScaledDepthBias   = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        rasterizerDesc.DepthClipEnable        = true;
        rasterizerDesc.MultisampleEnable      = false;
        rasterizerDesc.AntialiasedLineEnable  = false;
        rasterizerDesc.ForcedSampleCount      = 0;
        rasterizerDesc.ConservativeRaster     = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

        // Depth stencil state
        D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
        depthStencilDesc.DepthEnable        = true;
        depthStencilDesc.DepthWriteMask     = D3D12_DEPTH_WRITE_MASK_ALL;
        depthStencilDesc.DepthFunc          = D3D12_COMPARISON_FUNC_LESS;
        depthStencilDesc.StencilEnable      = false;
        depthStencilDesc.StencilReadMask    = D3D12_DEFAULT_STENCIL_READ_MASK;
        depthStencilDesc.StencilWriteMask   = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        depthStencilDesc.FrontFace          = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
        depthStencilDesc.BackFace           = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };

        // Input layout
        std::vector<InputLayoutElement> inputLayoutData =
        {
            { "Position",   DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "Normal",     DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "Tangent",    DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "TexCoord",   DXGI_FORMAT_R32G32_FLOAT,       sizeof(float) * 2 }
        };

        InputLayout inputLayout;
        inputLayout.Init(m_Renderer, inputLayoutData.data(), inputLayoutData.size());

        D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
        inputLayoutDesc.pInputElementDescs  = inputLayout.GetData();
        inputLayoutDesc.NumElements         = inputLayout.GetCount();

        // Cached PSO
        D3D12_CACHED_PIPELINE_STATE cachedPipelineState{};
        cachedPipelineState.pCachedBlob             = nullptr;
        cachedPipelineState.CachedBlobSizeInBytes   = 0;

        // Flags
#if defined(SPIELER_DEBUG)
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#else
        const D3D12_PIPELINE_STATE_FLAGS flags = D3D12_PIPELINE_STATE_FLAG_NONE;
#endif

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc{};
        desc.pRootSignature         = m_RootSignature.GetRaw();
        desc.VS                     = vsDesc;
        desc.PS                     = psDesc;
        desc.DS                     = {};
        desc.HS                     = {};
        desc.GS                     = {};
        desc.StreamOutput           = streamOutputDesc;
        desc.BlendState             = blendDesc;
        desc.SampleMask             = 0xffffffff;
        desc.RasterizerState        = rasterizerDesc;
        desc.DepthStencilState      = depthStencilDesc;
        desc.InputLayout            = inputLayoutDesc;
        desc.IBStripCutValue        = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        desc.PrimitiveTopologyType  = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        desc.NumRenderTargets       = 1;
        desc.RTVFormats[0]          = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.DSVFormat              = DXGI_FORMAT_D24_UNORM_S8_UINT;
        desc.SampleDesc.Count       = 1; // Use 4xMSAA
        desc.SampleDesc.Quality     = 0; // Use 4xMSAA
        desc.NodeMask               = 0;
        desc.CachedPSO              = cachedPipelineState;
        desc.Flags                  = flags;

        SPIELER_RETURN_IF_FAILED(m_Renderer.m_Device->CreateGraphicsPipelineState(&desc, __uuidof(ID3D12PipelineState), &m_PipelineState));

        return true;
    }

    void Application::InitViewMatrix()
    {
        m_CameraPosition    = DirectX::XMVectorSet(0.0f, 0.0f, 0.1f, 1.0f);
        m_CameraTarget      = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        m_CameraUpDirection = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

        m_View = DirectX::XMMatrixLookAtLH(m_CameraPosition, m_CameraTarget, m_CameraUpDirection);
    }

    void Application::InitProjectionMatrix()
    {
        m_Projection = DirectX::XMMatrixPerspectiveFovLH(0.5f * DirectX::XM_PI, m_Window.GetAspectRatio(), 0.1f, 1000.0f);
    }

    void Application::InitViewport()
    {
        m_Viewport.X        = 0.0f;
        m_Viewport.Y        = 0.0f;
        m_Viewport.Width    = m_Window.GetWidth();
        m_Viewport.Height   = m_Window.GetHeight();
        m_Viewport.MinDepth = 0.0f;
    }

    void Application::InitScissorRect()
    {
        m_ScissorRect.X         = 0.0f;
        m_ScissorRect.Y         = 0.0f;
        m_ScissorRect.Width     = m_Window.GetWidth();
        m_ScissorRect.Height    = m_Window.GetHeight();
    }

    void Application::OnUpdate(float dt)
    {
        static float angle = 0.0f;

        angle += 0.1f * dt;

        if (angle >= 360.0f)
        {
            angle = 0.0f;
        }

        DirectX::XMMATRIX world = 
            DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) *
            DirectX::XMMatrixRotationX(angle) *
            DirectX::XMMatrixRotationY(angle) *
            DirectX::XMMatrixRotationZ(angle) *
            DirectX::XMMatrixTranslation(0.0f, 0.0f, -100.0f);

        m_ConstantBuffer.GetData().WorldViewProjectionMatrix = DirectX::XMMatrixTranspose(world * m_View * m_Projection);
    }

    bool Application::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineState.Get()));
        {
            m_Renderer.SetViewport(m_Viewport);
            m_Renderer.SetScissorRect(m_ScissorRect);

            const std::uint32_t currentBackBuffer = m_Renderer.m_SwapChain3->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);

            m_Renderer.ClearRenderTarget(m_ClearColor);
            m_Renderer.ClearDepthStencil(1.0f, 0);

            auto a = m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(currentBackBuffer);
            auto b = m_Renderer.m_DSVDescriptorHeap.GetCPUHandle(0);

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_CBVDescriptorHeap.Bind();

            m_Renderer.m_CommandList->SetGraphicsRootSignature(m_RootSignature.GetRaw());
            m_Renderer.m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_Renderer.m_CommandList->SetGraphicsRootDescriptorTable(0, m_CBVDescriptorHeap.GetGPUHandle(0));

            m_Renderer.DrawIndexed(m_VertexBuffer, m_IndexBuffer);

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);

            SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList(false));
        }

#if SPIELER_USE_IMGUI
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            m_Renderer.SetViewport(m_Viewport);
            m_Renderer.SetScissorRect(m_ScissorRect);

            const std::uint32_t currentBackBuffer = m_Renderer.m_SwapChain3->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);

            auto a = m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(currentBackBuffer);
            auto b = m_Renderer.m_DSVDescriptorHeap.GetCPUHandle(0);

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_ImGuiDescriptorHeap.Bind();
            ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_Renderer.m_CommandList.Get());

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList(false));
#endif

        SPIELER_RETURN_IF_FAILED(m_Renderer.SwapBuffers(m_VSyncState));
        SPIELER_RETURN_IF_FAILED(m_Renderer.FlushCommandQueue());

        return true;
    }

    void Application::OnImGuiRender(float dt)
    {
        if (m_IsShowDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_IsShowDemoWindow);
        }

        {
            ImGui::Begin("Settings");

            ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&m_ClearColor));
            ImGui::Separator();
            ImGui::Text("FPS: %.1f, ms: %.3f", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Checkbox("VSync", reinterpret_cast<bool*>(&m_VSyncState));

            ImGui::End();
        }
    }

    void Application::OnResize()
    {
        m_Renderer.ResizeBuffers(m_Window.GetWidth(), m_Window.GetHeight());

        InitViewport();
        InitScissorRect();
        InitProjectionMatrix();
    }

    void Application::OnClose()
    {
        m_IsRunning = false;

#if SPIELER_USE_IMGUI
        ImGui_ImplDX12_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
#endif
    }

    void Application::WindowEventCallback(Event& event)
    {
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowCloseEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowClose));
        dispatcher.Dispatch<WindowMinimizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMinimized));
        dispatcher.Dispatch<WindowMaximizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowMaximized));
        dispatcher.Dispatch<WindowRestoredEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowRestored));
        dispatcher.Dispatch<WindowEnterResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowEnterResizing));
        dispatcher.Dispatch<WindowExitResizingEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowExitResizing));
        dispatcher.Dispatch<WindowFocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowFocused));
        dispatcher.Dispatch<WindowUnfocusedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowUnfocused));
        dispatcher.Dispatch<KeyPressedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnKeyPressed));
    }

    bool Application::OnWindowClose(WindowCloseEvent& event)
    {
        OnClose();

        return true;
    }

    bool Application::OnWindowMinimized(WindowMinimizedEvent& event)
    {
        m_IsPaused      = true;
        m_IsMinimized   = true;
        m_IsMaximized   = false;

        return true;
    }

    bool Application::OnWindowMaximized(WindowMaximizedEvent& event)
    {
        m_IsPaused      = false;
        m_IsMinimized   = false;
        m_IsMaximized   = true;

        return true;
    }

    bool Application::OnWindowRestored(WindowRestoredEvent& event)
    {
        if (m_IsMinimized)
        {
            m_IsPaused      = false;
            m_IsMinimized   = false;

            OnResize();
        }
        else if (m_IsMaximized)
        {
            m_IsPaused      = false;
            m_IsMaximized   = false;

            OnResize();
        }
        else if(m_IsResizing)
        {

        }
        else
        {
            OnResize();
        }

        return true;
    }

    bool Application::OnWindowEnterResizing(WindowEnterResizingEvent& event)
    {
        m_IsPaused      = true;
        m_IsResizing    = true;

        m_Timer.Stop();

        return true;
    }

    bool Application::OnWindowExitResizing(WindowExitResizingEvent& event)
    {
        m_IsPaused      = false;
        m_IsResizing    = false;

        m_Timer.Start();

        OnResize();

        return true;
    }

    bool Application::OnWindowFocused(WindowFocusedEvent& event)
    {
        m_IsPaused = false;

        m_Timer.Start();

        return true;
    }

    bool Application::OnWindowUnfocused(WindowUnfocusedEvent& event)
    {
        m_IsPaused = true;

        m_Timer.Stop();

        return true;
    }

    bool Application::OnKeyPressed(KeyPressedEvent& event)
    {
        if (event.GetKeyCode() == KeyCode_Escape)
        {
            OnClose();
        }

        if (event.GetKeyCode() == KeyCode_F1)
        {
            m_IsShowDemoWindow = !m_IsShowDemoWindow;
        }

        return true;
    }

    void Application::CalcStats()
    {
        static const float limit        = 1.0f;
        static const float coefficient  = 1.0f / limit;

        static std::uint32_t    fps         = 0;
        static float            timeElapsed = 0.0f;

        fps++;

        if (float delta = m_Timer.GetTotalTime() - timeElapsed; delta >= limit)
        {
            m_Window.SetTitle("FPS: " + std::to_string(fps * coefficient) + " ms: " + std::to_string(1000.0f / coefficient / fps));

            fps          = 0;
            timeElapsed += delta;
        }
    }

} // namespace Spieler