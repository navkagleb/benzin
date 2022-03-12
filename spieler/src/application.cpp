#include "application.h"

#include <fstream>
#include <numbers>

#include <DirectXColors.h>

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
        SPIELER_RETURN_IF_FAILED(m_CBVDescriptorHeap.Init(DescriptorHeapType_CBV, 2));
        SPIELER_RETURN_IF_FAILED(m_ImGuiDescriptorHeap.Init(DescriptorHeapType_CBV, 1));

#if SPIELER_USE_IMGUI
        SPIELER_RETURN_IF_FAILED(InitImGui());
#endif

        m_Renderer.ResetCommandList();

        SPIELER_RETURN_IF_FAILED(InitMSAAQualitySupport());

        InitViewport();
        InitScissorRect();
        InitRenderItems();

        RootDescriptor rootDescriptor;
        rootDescriptor.ShaderRegister   = 0;
        rootDescriptor.RegisterSpace    = 0;

        RootDescriptorTable rootDescriptorTable;
        rootDescriptorTable.DescriptorRanges = { DescriptorRange{ DescriptorRangeType_CBV, 1 } };

        RootParameter rootParameter;
        rootParameter.Type              = RootParameterType_DescriptorTable;
        rootParameter.ShaderVisibility  = ShaderVisibility_All;
        rootParameter.Child             = rootDescriptorTable;

        SPIELER_RETURN_IF_FAILED(m_RootSignature.Init({ rootParameter }));
        SPIELER_RETURN_IF_FAILED(InitMeshGeometry());

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

#if SPIELER_USE_IMGUI
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
#endif

    bool Application::InitPipelineState()
    {
        // Vertex shader
        SPIELER_RETURN_IF_FAILED(m_VertexShader.LoadFromFile(L"assets/shaders/basic_vertex_color.fx", "VS_Main"));
        SPIELER_RETURN_IF_FAILED(m_PixelShader.LoadFromFile(L"assets/shaders/basic_vertex_color.fx", "PS_Main"));
        
        RasterizerState rasterzerState;
        rasterzerState.FillMode = FillMode_Solid;
        rasterzerState.CullMode = CullMode_Back;

#if 0
        std::vector<InputLayoutElement> inputLayoutData =
        {
            { "Position",   DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "Normal",     DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "Tangent",    DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(float) * 3 },
            { "TexCoord",   DXGI_FORMAT_R32G32_FLOAT,       sizeof(float) * 2 }
        };
#endif

        std::vector<InputLayoutElement> inputLayoutData =
        {
            { "Position",   DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT4)    },
            { "Color",      DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMVECTORF32) }
        };

        InputLayout inputLayout;
        inputLayout.Init(m_Renderer, inputLayoutData.data(), inputLayoutData.size());

        PipelineStateProps pipelineStateProps;
        pipelineStateProps.RootSignature            = &m_RootSignature;
        pipelineStateProps.VertexShader             = &m_VertexShader;
        pipelineStateProps.PixelShader              = &m_PixelShader;
        pipelineStateProps.RasterizerState          = &rasterzerState;
        pipelineStateProps.InputLayout              = &inputLayout;
        pipelineStateProps.PrimitiveTopologyType    = PrimitiveTopologyType_Triangle;
        pipelineStateProps.RTVFormat                = m_Renderer.GetSwapChainProps().BufferFormat;
        pipelineStateProps.DSVFormat                = m_Renderer.GetDepthStencilFormat();
        
        SPIELER_RETURN_IF_FAILED(m_PipelineStates["solid"].Init(pipelineStateProps));

        RasterizerState rasterzerState1;
        rasterzerState1.FillMode = FillMode_Wireframe;
        rasterzerState1.CullMode = CullMode_None;

        pipelineStateProps.RasterizerState = &rasterzerState1;

        SPIELER_RETURN_IF_FAILED(m_PipelineStates["wireframe"].Init(pipelineStateProps));

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

    bool Application::InitMeshGeometry()
    {
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
        cylinderProps.TopRadius     = 10.0f;
        cylinderProps.BottomRadius  = 20.0f;
        cylinderProps.Height        = 40.0f;
        cylinderProps.SliceCount    = 20;
        cylinderProps.StackCount    = 3;

        SphereGeometryProps sphereProps;
        sphereProps.Radius      = 20.0f;
        sphereProps.SliceCount  = 30;
        sphereProps.StackCount  = 10;

        GeosphereGeometryProps geosphereProps;
        geosphereProps.Radius           = 20.0f;
        geosphereProps.SubdivisionCount = 1;

        const MeshData boxMeshData          = GeometryGenerator::GenerateBox<BasicVertex, std::uint32_t>(boxProps);
        const MeshData gridMeshData         = GeometryGenerator::GenerateGrid<BasicVertex, std::uint32_t>(gridProps);
        const MeshData cylinderMeshData     = GeometryGenerator::GenerateCylinder<BasicVertex, std::uint32_t>(cylinderProps);
        const MeshData sphereMeshData       = GeometryGenerator::GenerateSphere<BasicVertex, std::uint32_t>(sphereProps);
        const MeshData geosphereMeshData    = GeometryGenerator::GenerateGeosphere<BasicVertex, std::uint32_t>(geosphereProps);

        SubmeshGeometry& boxSubmesh     = m_Mesh.Submeshes["box"];
        boxSubmesh.IndexCount           = boxMeshData.Indices.size();
        boxSubmesh.StartIndexLocation   = 0;
        boxSubmesh.BaseVertexLocation   = 0;

        SubmeshGeometry& gridSubmesh    = m_Mesh.Submeshes["grid"];
        gridSubmesh.IndexCount          = gridMeshData.Indices.size();
        gridSubmesh.BaseVertexLocation  = boxSubmesh.BaseVertexLocation + boxMeshData.Vertices.size();
        gridSubmesh.StartIndexLocation  = boxSubmesh.StartIndexLocation + boxMeshData.Indices.size();

        SubmeshGeometry& cylinderSubmesh    = m_Mesh.Submeshes["cylinder"];
        cylinderSubmesh.IndexCount          = cylinderMeshData.Indices.size();
        cylinderSubmesh.BaseVertexLocation  = gridSubmesh.BaseVertexLocation + gridMeshData.Vertices.size();
        cylinderSubmesh.StartIndexLocation  = gridSubmesh.StartIndexLocation + gridMeshData.Indices.size();

        SubmeshGeometry& sphereSubmesh  = m_Mesh.Submeshes["sphere"];
        sphereSubmesh.IndexCount          = sphereMeshData.Indices.size();
        sphereSubmesh.BaseVertexLocation  = cylinderSubmesh.BaseVertexLocation + cylinderMeshData.Vertices.size();
        sphereSubmesh.StartIndexLocation  = cylinderSubmesh.StartIndexLocation + cylinderMeshData.Indices.size();

        SubmeshGeometry& geosphereSubmesh   = m_Mesh.Submeshes["geosphere"];
        geosphereSubmesh.IndexCount              = geosphereMeshData.Indices.size();
        geosphereSubmesh.BaseVertexLocation      = sphereSubmesh.BaseVertexLocation + sphereMeshData.Vertices.size();
        geosphereSubmesh.StartIndexLocation      = sphereSubmesh.StartIndexLocation + sphereMeshData.Indices.size();

        const std::uint32_t vertexCount = boxMeshData.Vertices.size() + gridMeshData.Vertices.size() + cylinderMeshData.Vertices.size() +  sphereMeshData.Vertices.size() + geosphereMeshData.Vertices.size();
        const std::uint32_t indexCount  = boxMeshData.Indices.size() + gridMeshData.Indices.size() + cylinderMeshData.Indices.size() + sphereMeshData.Indices.size() + geosphereMeshData.Indices.size();

        // Vertices
        struct Vertex
        {
            DirectX::XMFLOAT3       Position;
            DirectX::XMVECTORF32    Color;
        };

        std::vector<Vertex> vertices;
        vertices.reserve(vertexCount);

        for (const auto& vertex : boxMeshData.Vertices)
        {
            vertices.emplace_back(vertex.Position, DirectX::Colors::OrangeRed);
        }

        for (const auto& vertex : gridMeshData.Vertices)
        {
            vertices.emplace_back(vertex.Position, DirectX::Colors::Bisque);
        }

        for (const auto& vertex : cylinderMeshData.Vertices)
        {
            vertices.emplace_back(vertex.Position, DirectX::Colors::Coral);
        }

        for (const auto& vertex : sphereMeshData.Vertices)
        {
            vertices.emplace_back(vertex.Position, DirectX::Colors::LawnGreen);
        }

        for (const auto& vertex : geosphereMeshData.Vertices)
        {
            vertices.emplace_back(vertex.Position, DirectX::Colors::LimeGreen);
        }

        // Indices
        std::vector<std::uint32_t> indices;
        indices.reserve(indexCount);
        indices.insert(indices.end(), boxMeshData.Indices.begin(),       boxMeshData.Indices.end());
        indices.insert(indices.end(), gridMeshData.Indices.begin(),      gridMeshData.Indices.end());
        indices.insert(indices.end(), cylinderMeshData.Indices.begin(),  cylinderMeshData.Indices.end());
        indices.insert(indices.end(), sphereMeshData.Indices.begin(),    sphereMeshData.Indices.end());
        indices.insert(indices.end(), geosphereMeshData.Indices.begin(), geosphereMeshData.Indices.end());

        SPIELER_RETURN_IF_FAILED(m_Mesh.VertexBuffer.Init(vertices.data(), vertices.size()));
        SPIELER_RETURN_IF_FAILED(m_Mesh.IndexBuffer.Init(indices.data(), indices.size()));

        m_Mesh.PrimitiveTopology = PrimitiveTopology_TriangleList;

        return true;
    }

    bool Application::InitRenderItems()
    {
        m_ConstantBuffers.resize(2);

        RenderItem box;
        box.MeshGeometry        = &m_Mesh;
        box.SubmeshGeometry     = &m_Mesh.Submeshes["box"];
        box.ConstantBufferIndex = 0;
        
        SPIELER_RETURN_IF_FAILED(m_ConstantBuffers[0].Init(m_CBVDescriptorHeap.GetCPUHandle(0)));
        m_RenderItems.push_back(std::move(box));

        RenderItem cylinder;
        cylinder.MeshGeometry           = &m_Mesh;
        cylinder.SubmeshGeometry        = &m_Mesh.Submeshes["cylinder"];
        cylinder.ConstantBufferIndex    = 1;

        SPIELER_RETURN_IF_FAILED(m_ConstantBuffers[1].Init(m_CBVDescriptorHeap.GetCPUHandle(1)));
        m_RenderItems.push_back(std::move(cylinder));

        return true;
    }

    void Application::OnUpdate(float dt)
    {
        static float angle = 0.0f;

        angle += 0.1f * dt;

        if (angle >= 360.0f)
        {
            angle = 0.0f;
        }

        m_RenderItems[0].World = 
            DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) *
            DirectX::XMMatrixRotationX(angle) *
            DirectX::XMMatrixRotationY(angle) *
            DirectX::XMMatrixRotationZ(angle) *
            DirectX::XMMatrixTranslation(30.0f, 0.0f, -50.0f);

        m_RenderItems[1].World = 
            DirectX::XMMatrixScaling(1.0f, 1.0f, 1.0f) *
            DirectX::XMMatrixRotationX(angle) *
            DirectX::XMMatrixRotationY(angle) *
            DirectX::XMMatrixRotationZ(angle) *
            DirectX::XMMatrixTranslation(-30.0f, 0.0f, -50.0f);

        m_ConstantBuffers[0].GetData().WorldViewProjectionMatrix = DirectX::XMMatrixTranspose(m_RenderItems[0].World * m_View * m_Projection);
        m_ConstantBuffers[1].GetData().WorldViewProjectionMatrix = DirectX::XMMatrixTranspose(m_RenderItems[1].World * m_View * m_Projection);
    }

    bool Application::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_IsSolidRasterizerState ? m_PipelineStates["solid"] : m_PipelineStates["wireframe"]));
        {
            m_Renderer.SetViewport(m_Viewport);
            m_Renderer.SetScissorRect(m_ScissorRect);

            const std::uint32_t currentBackBuffer = m_Renderer.m_SwapChain3->GetCurrentBackBufferIndex();

            D3D12_RESOURCE_BARRIER barrier{};
            barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource    = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_RENDER_TARGET;

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

            m_RootSignature.Bind();
            m_CBVDescriptorHeap.Bind();
            m_Mesh.VertexBuffer.Bind();
            m_Mesh.IndexBuffer.Bind();
            m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(m_Mesh.PrimitiveTopology));

            for (const auto& renderItem : m_RenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                m_Renderer.m_CommandList->SetGraphicsRootDescriptorTable(0, m_CBVDescriptorHeap.GetGPUHandle(renderItem.ConstantBufferIndex));
                m_Renderer.m_CommandList->DrawIndexedInstanced(submeshGeometry.IndexCount, 1, submeshGeometry.StartIndexLocation, submeshGeometry.BaseVertexLocation, 0);
            }
            
            barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource    = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource  = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore  = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter   = D3D12_RESOURCE_STATE_PRESENT;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList(false));

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

#if SPIELER_USE_IMGUI
    void Application::OnImGuiRender(float dt)
    {
        if (m_IsShowDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_IsShowDemoWindow);
        }

        {
            ImGui::Begin("Settings");

            ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&m_ClearColor));
            ImGui::Checkbox("Solid rasterizer state", &m_IsSolidRasterizerState);
            ImGui::Separator();
            ImGui::Text("FPS: %.1f, ms: %.3f", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::Checkbox("VSync", reinterpret_cast<bool*>(&m_VSyncState));

            ImGui::End();
        }
    }
#endif

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

        OnResize();

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