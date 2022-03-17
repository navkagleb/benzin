#include "land_layer.h"

#include <random>
#include <chrono>

#include <DirectXColors.h>

#include <imgui/imgui.h>

#include <spieler/application.h>
#include <spieler/common.h>
#include <spieler/window/input.h>
#include <spieler/window/event_dispatcher.h>
#include <spieler/window/window_event.h>
#include <spieler/window/mouse_event.h>
#include <spieler/window/key_event.h>
#include <spieler/renderer/geometry_generator.h>

namespace Sandbox
{

    struct PassConstants
    {
        DirectX::XMMATRIX View{};
        DirectX::XMMATRIX Projection{};
    };

    struct ObjectConstants
    {
        DirectX::XMMATRIX World{};
    };

    namespace _Internal
    {

        float GetHeight(float x, float z)
        {
            return 0.3f * (z * std::sin(0.1f * x) + x * std::cos(0.1f * z));
        }

    } // namespace _Internal

    LandLayer::LandLayer(Spieler::Window& window, Spieler::Renderer& renderer)
        : m_Window(window)
        , m_Renderer(renderer)
        , m_CameraController(DirectX::XM_PIDIV2, m_Window.GetAspectRatio())
        , m_Waves(WavesProps{ 200, 200, 0.03f, 0.8f, 4.0f, 0.2f })
    {}

    bool LandLayer::OnAttach()
    {
        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitUploadBuffers());

        InitViewport();
        InitScissorRect();

        m_Renderer.ResetCommandList();
        {
            //SPIELER_RETURN_IF_FAILED(InitMeshGeometry());
            SPIELER_RETURN_IF_FAILED(InitLandGeometry());
            SPIELER_RETURN_IF_FAILED(InitWavesGeometry());
            SPIELER_RETURN_IF_FAILED(InitRootSignature());
            SPIELER_RETURN_IF_FAILED(InitPipelineState());
            SPIELER_RETURN_IF_FAILED(m_PassConstantBuffer.InitAsRootDescriptor<PassConstants>(&m_PassUploadBuffer, 0));
        }
        m_Renderer.ExexuteCommandList();

        return true;
    }

    void LandLayer::OnEvent(Spieler::Event& event)
    {
        Spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        dispatcher.Dispatch<Spieler::KeyPressedEvent>([&](Spieler::KeyPressedEvent& event)
        {
            if (event.GetKeyCode() == Spieler::KeyCode_F)
            {
                if (!::SetCapture(m_Window.GetHandle()))
                {
                    ::OutputDebugString("Fuck\n");
                }
            }
            return true;
        });

        m_CameraController.OnEvent(event);
    }

    void LandLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        const auto& camera = m_CameraController.GetCamera();
        m_PassConstantBuffer.As<PassConstants>().View       = DirectX::XMMatrixTranspose(camera.View);
        m_PassConstantBuffer.As<PassConstants>().Projection = DirectX::XMMatrixTranspose(camera.Projection);

        // Update Waves
        static float time = 0.0f;
        
        static std::seed_seq seedSequence
        {
            std::random_device()(),
            static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count())
        };

        static std::mt19937_64 mersenneTwisterEngine(seedSequence);

        if ((Spieler::Application::GetInstance().GetTimer().GetTotalTime() - time) >= 0.25f)
        {
            time += 0.25f;

            const std::uint32_t i = std::uniform_int_distribution<std::uint32_t>{ 4, m_Waves.GetRowCount() - 5 }(mersenneTwisterEngine);
            const std::uint32_t j = std::uniform_int_distribution<std::uint32_t>{ 4, m_Waves.GetColumnCount() - 5 }(mersenneTwisterEngine);

            const float r = std::uniform_real_distribution<float>{ 0.2f, 0.5f }(mersenneTwisterEngine);

            m_Waves.Disturb(i, j, r);
        }

        m_Waves.OnUpdate(dt);

        for (std::uint32_t i = 0; i < m_Waves.GetVertexCount(); ++i)
        {
            m_WavesUploadBuffer.As<ColorVertex>(i).Position = m_Waves.GetPosition(i);
            m_WavesUploadBuffer.As<ColorVertex>(i).Color    = DirectX::XMFLOAT4{ DirectX::Colors::Blue };
        }

        m_MeshGeometries["waves"].VertexBuffer.Init(m_WavesUploadBuffer);
    }

    bool LandLayer::OnRender(float dt)
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

            const auto& landGeometry = m_MeshGeometries["land"];

            m_RootSignature.Bind();
            m_CBVDescriptorHeap.Bind();
            m_PassConstantBuffer.BindAsRootDescriptor(0);

            for (const auto& renderItem : m_RenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                renderItem.MeshGeometry->VertexBuffer.Bind();
                renderItem.MeshGeometry->IndexBuffer.Bind();
                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->PrimitiveTopology));

                renderItem.ConstantBuffer.BindAsRootDescriptor(1);
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

        return true;
    }

    void LandLayer::OnImGuiRender(float dt)
    {
        if (m_IsShowDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_IsShowDemoWindow);
        }

        ImGui::Begin("Settings");
        {
            ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&m_ClearColor));
            ImGui::Checkbox("Solid rasterizer state", &m_IsSolidRasterizerState);
            ImGui::Separator();
            ImGui::Text("FPS: %.1f, ms: %.3f", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        }
        ImGui::End();

        m_CameraController.OnImGuiRender(dt);
    }

    bool LandLayer::InitDescriptorHeaps()
    {
        SPIELER_RETURN_IF_FAILED(m_CBVDescriptorHeap.Init(Spieler::DescriptorHeapType_CBV, 4));

        return true;
    }

    bool LandLayer::InitUploadBuffers()
    {
        SPIELER_RETURN_IF_FAILED(m_PassUploadBuffer.Init<PassConstants>(Spieler::UploadBufferType_ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_ObjectUploadBuffer.Init<ObjectConstants>(Spieler::UploadBufferType_ConstantBuffer, m_RenderItemCount));
        SPIELER_RETURN_IF_FAILED(m_WavesUploadBuffer.Init<ColorVertex>(Spieler::UploadBufferType_Default, m_Waves.GetVertexCount()));

        return true;
    }

    bool LandLayer::InitMeshGeometry()
    {
        Spieler::BoxGeometryProps boxProps;
        boxProps.Width              = 30.0f;
        boxProps.Height             = 20.0f;
        boxProps.Depth              = 25.0f;
        boxProps.SubdivisionCount   = 2;

        Spieler::GridGeometryProps gridProps;
        gridProps.Width         = 100.0f;
        gridProps.Depth         = 100.0f;
        gridProps.RowCount      = 10;
        gridProps.ColumnCount   = 10;

        Spieler::CylinderGeometryProps cylinderProps;
        cylinderProps.TopRadius     = 10.0f;
        cylinderProps.BottomRadius  = 20.0f;
        cylinderProps.Height        = 40.0f;
        cylinderProps.SliceCount    = 20;
        cylinderProps.StackCount    = 3;

        Spieler::SphereGeometryProps sphereProps;
        sphereProps.Radius      = 20.0f;
        sphereProps.SliceCount  = 30;
        sphereProps.StackCount  = 10;

        Spieler::GeosphereGeometryProps geosphereProps;
        geosphereProps.Radius           = 20.0f;
        geosphereProps.SubdivisionCount = 2;

        const Spieler::MeshData boxMeshData          = Spieler::GeometryGenerator::GenerateBox<Spieler::BasicVertex, std::uint32_t>(boxProps);
        const Spieler::MeshData gridMeshData         = Spieler::GeometryGenerator::GenerateGrid<Spieler::BasicVertex, std::uint32_t>(gridProps);
        const Spieler::MeshData cylinderMeshData     = Spieler::GeometryGenerator::GenerateCylinder<Spieler::BasicVertex, std::uint32_t>(cylinderProps);
        const Spieler::MeshData sphereMeshData       = Spieler::GeometryGenerator::GenerateSphere<Spieler::BasicVertex, std::uint32_t>(sphereProps);
        const Spieler::MeshData geosphereMeshData    = Spieler::GeometryGenerator::GenerateGeosphere<Spieler::BasicVertex, std::uint32_t>(geosphereProps);

        auto& meshes = m_MeshGeometries["meshes"];

        Spieler::SubmeshGeometry& boxSubmesh    = meshes.Submeshes["box"];
        boxSubmesh.IndexCount                   = static_cast<std::uint32_t>(boxMeshData.Indices.size());
        boxSubmesh.StartIndexLocation           = 0;
        boxSubmesh.BaseVertexLocation           = 0;

        Spieler::SubmeshGeometry& gridSubmesh   = meshes.Submeshes["grid"];
        gridSubmesh.IndexCount                  = static_cast<std::uint32_t>(gridMeshData.Indices.size());
        gridSubmesh.BaseVertexLocation          = boxSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(boxMeshData.Vertices.size());
        gridSubmesh.StartIndexLocation          = boxSubmesh.StartIndexLocation + static_cast<std::uint32_t>(boxMeshData.Indices.size());

        Spieler::SubmeshGeometry& cylinderSubmesh   = meshes.Submeshes["cylinder"];
        cylinderSubmesh.IndexCount                  = static_cast<std::uint32_t>(cylinderMeshData.Indices.size());
        cylinderSubmesh.BaseVertexLocation          = gridSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(gridMeshData.Vertices.size());
        cylinderSubmesh.StartIndexLocation          = gridSubmesh.StartIndexLocation + static_cast<std::uint32_t>(gridMeshData.Indices.size());

        Spieler::SubmeshGeometry& sphereSubmesh     = meshes.Submeshes["sphere"];
        sphereSubmesh.IndexCount                    = static_cast<std::uint32_t>(sphereMeshData.Indices.size());
        sphereSubmesh.BaseVertexLocation            = cylinderSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(cylinderMeshData.Vertices.size());
        sphereSubmesh.StartIndexLocation            = cylinderSubmesh.StartIndexLocation + static_cast<std::uint32_t>(cylinderMeshData.Indices.size());

        Spieler::SubmeshGeometry& geosphereSubmesh      = meshes.Submeshes["geosphere"];
        geosphereSubmesh.IndexCount                     = static_cast<std::uint32_t>(geosphereMeshData.Indices.size());
        geosphereSubmesh.BaseVertexLocation             = sphereSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(sphereMeshData.Vertices.size());
        geosphereSubmesh.StartIndexLocation             = sphereSubmesh.StartIndexLocation + static_cast<std::uint32_t>(sphereMeshData.Indices.size());

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

        SPIELER_RETURN_IF_FAILED(meshes.VertexBuffer.Init(vertices.data(), static_cast<std::uint32_t>(vertices.size())));
        SPIELER_RETURN_IF_FAILED(meshes.IndexBuffer.Init(indices.data(), static_cast<std::uint32_t>(indices.size())));

        meshes.VertexBuffer.SetName(L"MeshGeometry VertexBuffer");
        meshes.IndexBuffer.SetName(L"MeshGeometry IndexBuffer");

        meshes.PrimitiveTopology = Spieler::PrimitiveTopology_TriangleList;

        // Render items
        Spieler::RenderItem box;
        box.MeshGeometry        = &meshes;
        box.SubmeshGeometry     = &meshes.Submeshes["box"];
        
        SPIELER_RETURN_IF_FAILED(box.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 0));
        m_RenderItems.push_back(std::move(box));

        Spieler::RenderItem cylinder;
        cylinder.MeshGeometry           = &meshes;
        cylinder.SubmeshGeometry        = &meshes.Submeshes["cylinder"];

        SPIELER_RETURN_IF_FAILED(cylinder.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 1));
        m_RenderItems.push_back(std::move(cylinder));

        Spieler::RenderItem sphere;
        sphere.MeshGeometry           = &meshes;
        sphere.SubmeshGeometry        = &meshes.Submeshes["sphere"];

        SPIELER_RETURN_IF_FAILED(sphere.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 2));
        m_RenderItems.push_back(std::move(sphere));

        Spieler::RenderItem geosphere;
        geosphere.MeshGeometry           = &meshes;
        geosphere.SubmeshGeometry        = &meshes.Submeshes["geosphere"];

        SPIELER_RETURN_IF_FAILED(geosphere.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 3));
        m_RenderItems.push_back(std::move(geosphere));

        return true;
    }

    bool LandLayer::InitLandGeometry()
    {
        Spieler::GridGeometryProps gridGeometryProps;
        gridGeometryProps.Width         = 160.0f;
        gridGeometryProps.Depth         = 160.0f;
        gridGeometryProps.RowCount      = 100;
        gridGeometryProps.ColumnCount   = 100;

        const auto& gridMeshData = Spieler::GeometryGenerator::GenerateGrid<Spieler::BasicVertex, std::uint32_t>(gridGeometryProps);

        std::vector<ColorVertex> vertices;
        vertices.reserve(gridMeshData.Vertices.size());

        for (const auto& basicVertex : gridMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = basicVertex.Position;
            colorVertex.Position.y = _Internal::GetHeight(colorVertex.Position.x, colorVertex.Position.z);

            if (colorVertex.Position.y < -10.0f)
            {
                // Sandy beach color.
                colorVertex.Color = DirectX::XMFLOAT4{ 1.0f, 0.96f, 0.62f, 1.0f };
            }
            else if (colorVertex.Position.y < 5.0f)
            {
                // Light yellow-green.
                colorVertex.Color = DirectX::XMFLOAT4{ 0.48f, 0.77f, 0.46f, 1.0f };
            }
            else if (colorVertex.Position.y < 12.0f)
            {
                // Dark yellow-green.
                colorVertex.Color = DirectX::XMFLOAT4{ 0.1f, 0.48f, 0.19f, 1.0f };
            }
            else if (colorVertex.Position.y < 20.0f)
            {
                // Dark brown.
                colorVertex.Color = DirectX::XMFLOAT4{ 0.45f, 0.39f, 0.34f, 1.0f };
            }
            else
            {
                // White snow.
                colorVertex.Color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            }

            vertices.push_back(colorVertex);
        }

        auto& landMeshGeometry = m_MeshGeometries["land"];

        SPIELER_RETURN_IF_FAILED(landMeshGeometry.VertexBuffer.Init(vertices.data(), vertices.size()));
        SPIELER_RETURN_IF_FAILED(landMeshGeometry.IndexBuffer.Init(gridMeshData.Indices.data(), gridMeshData.Indices.size()));

        landMeshGeometry.VertexBuffer.SetName(L"LandMeshGeometry VertexBuffer");
        landMeshGeometry.IndexBuffer.SetName(L"LandMeshGeometry IndexBuffer");

        landMeshGeometry.PrimitiveTopology = Spieler::PrimitiveTopology_TriangleList;

        auto& landMesh      = landMeshGeometry.Submeshes["land"];
        landMesh.IndexCount = gridMeshData.Indices.size();

        // Render items
        Spieler::RenderItem land;
        land.MeshGeometry       = &landMeshGeometry;
        land.SubmeshGeometry    = &landMeshGeometry.Submeshes["land"];

        SPIELER_RETURN_IF_FAILED(land.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 0));
        land.ConstantBuffer.As<ObjectConstants>().World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -20.0f, 0.0f));

        m_RenderItems.push_back(std::move(land));

        return true;
    }

    bool LandLayer::InitWavesGeometry()
    {
        std::vector<std::uint32_t> indices(m_Waves.GetTriangleCount() * 3);

        const std::uint32_t m = m_Waves.GetRowCount();
        const std::uint32_t n = m_Waves.GetColumnCount();

        std::uint32_t k = 0;

        for (std::uint32_t i = 0; i < m - 1; ++i)
        {
            for (std::uint32_t j = 0; j < n - 1; ++j)
            {
                indices[k + 0] = i * n + j;
                indices[k + 1] = i * n + j + 1;
                indices[k + 2] = (i + 1) * n + j;

                indices[k + 3] = (i + 1) * n + j;
                indices[k + 4] = i * n + j + 1;
                indices[k + 5] = (i + 1) * n + j + 1;

                k += 6;
            }
        }

        auto& wavesMeshGeometry = m_MeshGeometries["waves"];

        SPIELER_RETURN_IF_FAILED(wavesMeshGeometry.IndexBuffer.Init(indices.data(), static_cast<std::uint32_t>(indices.size())));
        
        wavesMeshGeometry.PrimitiveTopology = Spieler::PrimitiveTopology_TriangleList;

        auto& submesh               = wavesMeshGeometry.Submeshes["main"];
        submesh.IndexCount          = static_cast<std::uint32_t>(indices.size());
        submesh.StartIndexLocation  = 0;
        submesh.BaseVertexLocation  = 0;

        // Render Item
        Spieler::RenderItem waves;
        waves.MeshGeometry       = &wavesMeshGeometry;
        waves.SubmeshGeometry    = &wavesMeshGeometry.Submeshes["main"];

        SPIELER_RETURN_IF_FAILED(waves.ConstantBuffer.InitAsRootDescriptor<ObjectConstants>(&m_ObjectUploadBuffer, 1));
        waves.ConstantBuffer.As<ObjectConstants>().World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -25.0f, 0.0f));

        m_RenderItems.push_back(std::move(waves));

        return true;
    }

    bool LandLayer::InitRootSignature()
    {
        std::vector<Spieler::RootParameter> rootParameters(2);

        rootParameters[0].Type              = Spieler::RootParameterType_ConstantBufferView;
        rootParameters[0].ShaderVisibility  = Spieler::ShaderVisibility_All;
        rootParameters[0].Child             = Spieler::RootDescriptor{ 0, 0 };

        rootParameters[1].Type              = Spieler::RootParameterType_ConstantBufferView;
        rootParameters[1].ShaderVisibility  = Spieler::ShaderVisibility_All;
        rootParameters[1].Child             = Spieler::RootDescriptor{ 1, 0 };

        SPIELER_RETURN_IF_FAILED(m_RootSignature.Init(rootParameters));
        
        return true;
    }

    bool LandLayer::InitPipelineState()
    {
        // Vertex shader
        SPIELER_RETURN_IF_FAILED(m_VertexShader.LoadFromFile(L"assets/shaders/basic_vertex_color.fx", "VS_Main"));
        SPIELER_RETURN_IF_FAILED(m_PixelShader.LoadFromFile(L"assets/shaders/basic_vertex_color.fx", "PS_Main"));
        
        Spieler::RasterizerState rasterzerState;
        rasterzerState.FillMode = Spieler::FillMode_Solid;
        rasterzerState.CullMode = Spieler::CullMode_Back;

        std::vector<Spieler::InputLayoutElement> inputLayoutData =
        {
            { "Position",   DXGI_FORMAT_R32G32B32_FLOAT,    sizeof(DirectX::XMFLOAT3) },
            { "Color",      DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT4) }
        };

        Spieler::InputLayout inputLayout;
        inputLayout.Init(m_Renderer, inputLayoutData.data(), inputLayoutData.size());

        Spieler::PipelineStateProps pipelineStateProps;
        pipelineStateProps.RootSignature            = &m_RootSignature;
        pipelineStateProps.VertexShader             = &m_VertexShader;
        pipelineStateProps.PixelShader              = &m_PixelShader;
        pipelineStateProps.RasterizerState          = &rasterzerState;
        pipelineStateProps.InputLayout              = &inputLayout;
        pipelineStateProps.PrimitiveTopologyType    = Spieler::PrimitiveTopologyType_Triangle;
        pipelineStateProps.RTVFormat                = m_Renderer.GetSwapChainProps().BufferFormat;
        pipelineStateProps.DSVFormat                = m_Renderer.GetDepthStencilFormat();
        
        SPIELER_RETURN_IF_FAILED(m_PipelineStates["solid"].Init(pipelineStateProps));

        Spieler::RasterizerState rasterzerState1;
        rasterzerState1.FillMode = Spieler::FillMode_Wireframe;
        rasterzerState1.CullMode = Spieler::CullMode_None;

        pipelineStateProps.RasterizerState = &rasterzerState1;

        SPIELER_RETURN_IF_FAILED(m_PipelineStates["wireframe"].Init(pipelineStateProps));

        return true;
    }

    void LandLayer::InitViewport()
    {
        m_Viewport.X        = 0.0f;
        m_Viewport.Y        = 0.0f;
        m_Viewport.Width    = static_cast<float>(m_Window.GetWidth());
        m_Viewport.Height   = static_cast<float>(m_Window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void LandLayer::InitScissorRect()
    {
        m_ScissorRect.X         = 0.0f;
        m_ScissorRect.Y         = 0.0f;
        m_ScissorRect.Width     = static_cast<float>(m_Window.GetWidth());
        m_ScissorRect.Height    = static_cast<float>(m_Window.GetHeight());
    }

    bool LandLayer::OnWindowResized(Spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();

        return false;
    }

} // namespace Sandbox