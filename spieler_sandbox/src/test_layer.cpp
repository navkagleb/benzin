#include "test_layer.hpp"

#include <spieler/window/event.hpp>
#include <spieler/window/event_dispatcher.hpp>
#include <spieler/renderer/geometry_generator.hpp>

namespace Sandbox
{

    TestLayer::TestLayer(Spieler::Window& window, Spieler::Renderer& renderer)
        : m_Window(window)
        , m_Renderer(renderer)
        , m_CameraController(DirectX::XM_PIDIV2, m_Window.GetAspectRatio())
    {}

    bool TestLayer::OnAttach()
    {
        UpdateViewport();
        UpdateScissorRect();

        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitUploadBuffers());

        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures());
            SPIELER_RETURN_IF_FAILED(InitMeshGeometries());
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList());

        SPIELER_RETURN_IF_FAILED(InitRootSignatures());
        SPIELER_RETURN_IF_FAILED(InitPipelineStates());

        InitConstantBuffers();

        return true;
    }

    void TestLayer::OnEvent(Spieler::Event& event)
    {
        Spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        m_CameraController.OnEvent(event);
    }

    void TestLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        const ProjectionCamera& camera{ m_CameraController.GetCamera() };
        
        auto& passConstants{ m_ConstantBuffers["pass"].As<PassConstants>() };
        passConstants.View = DirectX::XMMatrixTranspose(camera.View);
        passConstants.Projection = DirectX::XMMatrixTranspose(camera.Projection);
    }

    bool TestLayer::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineStates["default"]));
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

            m_Renderer.ClearRenderTarget({ 0.1f, 0.1f, 0.1f, 0.1f });
            m_Renderer.ClearDepthStencil(1.0f, 0);

            auto a = m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetCPUHandle(currentBackBuffer);
            auto b = m_Renderer.m_DSVDescriptorHeap.GetCPUHandle(0);

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_RootSignatures["default"].Bind();
            m_ConstantBuffers["pass"].BindAsRootDescriptor(0);

            m_DescriptorHeaps["srv"].Bind();
            m_SRVs["wood_crate1"].Bind(2);

            for (const auto& [renderItemName, renderItem] : m_RenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                renderItem.MeshGeometry->VertexBuffer.Bind();
                renderItem.MeshGeometry->IndexBuffer.Bind();
                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->PrimitiveTopology));

                renderItem.ConstantBuffer.BindAsRootDescriptor(1);
                
                m_Renderer.m_CommandList->DrawIndexedInstanced(submeshGeometry.IndexCount, 1, submeshGeometry.StartIndexLocation, submeshGeometry.BaseVertexLocation, 0);
            }

            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_Renderer.GetSwapChainBuffer(currentBackBuffer).Get();
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

            m_Renderer.m_CommandList->ResourceBarrier(1, &barrier);
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList());

        return true;
    }

    void TestLayer::OnImGuiRender(float dt)
    {
        m_CameraController.OnImGuiRender(dt);
    }

    bool TestLayer::InitDescriptorHeaps()
    {
        SPIELER_RETURN_IF_FAILED(m_DescriptorHeaps["srv"].Init(Spieler::DescriptorHeapType_SRV, 1));

        return true;
    }

    bool TestLayer::InitUploadBuffers()
    {
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["pass"].Init<PassConstants>(Spieler::UploadBufferType_ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["object"].Init<ObjectConstants>(Spieler::UploadBufferType_ConstantBuffer, 1));

        return true;
    }

    bool TestLayer::InitTextures()
    {
        // Wood texture
        {
            const std::string woodTextureName{ "wood_crate1" };

            SPIELER_RETURN_IF_FAILED(m_Textures[woodTextureName].LoadFromDDSFile(L"assets/textures/wood_crate1.dds"));

            m_SRVs[woodTextureName].Init(m_Textures[woodTextureName], m_DescriptorHeaps["srv"], 0);
        }
        
        return true;
    }

    bool TestLayer::InitMeshGeometries()
    {
        Spieler::BoxGeometryProps boxGeometryProps;
        boxGeometryProps.Width = 50.0f;
        boxGeometryProps.Height = 50.0f;
        boxGeometryProps.Depth = 50.0f;

        const auto& boxData{ Spieler::GeometryGenerator::GenerateBox<Spieler::BasicVertex, std::uint32_t>(boxGeometryProps) };

        Spieler::MeshGeometry& boxMeshGeometry{ m_MeshGeometries["box"] };
        boxMeshGeometry.PrimitiveTopology = Spieler::PrimitiveTopology_TriangleList;
        SPIELER_RETURN_IF_FAILED(boxMeshGeometry.VertexBuffer.Init(boxData.Vertices.data(), static_cast<std::uint32_t>(boxData.Vertices.size())));
        SPIELER_RETURN_IF_FAILED(boxMeshGeometry.IndexBuffer.Init(boxData.Indices.data(), static_cast<std::uint32_t>(boxData.Indices.size())));

        Spieler::SubmeshGeometry& boxSubmeshGeometry{ boxMeshGeometry.Submeshes["grid"] };
        boxSubmeshGeometry.IndexCount = static_cast<std::uint32_t>(boxData.Indices.size());
        boxSubmeshGeometry.BaseVertexLocation = 0;
        boxSubmeshGeometry.StartIndexLocation = 0;

        Spieler::RenderItem& boxRenderItem{ m_RenderItems["box"] };
        boxRenderItem.MeshGeometry = &boxMeshGeometry;
        boxRenderItem.SubmeshGeometry = &boxSubmeshGeometry;
        boxRenderItem.ConstantBuffer.InitAsRootDescriptor(&m_UploadBuffers["object"], 0);

        boxRenderItem.ConstantBuffer.As<ObjectConstants>().World = DirectX::XMMatrixTranspose(
            DirectX::XMMatrixTranslation(0.0, 0.0f, 100.0f)
        );

        return true;
    }

    bool TestLayer::InitRootSignatures()
    {
        std::vector<Spieler::RootParameter> rootParameters(3);
        rootParameters[0].Type = Spieler::RootParameterType_ConstantBufferView;
        rootParameters[0].ShaderVisibility = Spieler::ShaderVisibility_All;
        rootParameters[0].Child = Spieler::RootDescriptor{ 0, 0 };

        rootParameters[1].Type = Spieler::RootParameterType_ConstantBufferView;
        rootParameters[1].ShaderVisibility = Spieler::ShaderVisibility_All;
        rootParameters[1].Child = Spieler::RootDescriptor{ 1, 0 };

        rootParameters[2].Type = Spieler::RootParameterType_DescriptorTable;
        rootParameters[2].ShaderVisibility = Spieler::ShaderVisibility_All;
        rootParameters[2].Child = Spieler::RootDescriptorTable{ { Spieler::DescriptorRange{ Spieler::DescriptorRangeType_SRV, 1 } } };

        std::vector<Spieler::StaticSampler> staticSamplers(1);
        staticSamplers[0] = Spieler::StaticSampler(Spieler::TextureFilterType_Point, Spieler::TextureAddressMode_Wrap);

        Spieler::RootSignature& rootSignature{ m_RootSignatures["default"] };
        SPIELER_RETURN_IF_FAILED(m_RootSignatures["default"].Init(rootParameters, staticSamplers));

        return true;
    }

    bool TestLayer::InitPipelineStates()
    {
        SPIELER_RETURN_IF_FAILED(m_VertexShaders["texture"].LoadFromFile(L"assets/shaders/texture.fx", "VS_Main"));
        SPIELER_RETURN_IF_FAILED(m_PixelShaders["texture"].LoadFromFile(L"assets/shaders/texture.fx", "PS_Main"));

        Spieler::RasterizerState rasterzerState;
        rasterzerState.FillMode = Spieler::FillMode_Solid;
        rasterzerState.CullMode = Spieler::CullMode_Back;

        Spieler::InputLayout inputLayout
        {
            Spieler::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            Spieler::InputLayoutElement{ "Normal", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            Spieler::InputLayoutElement{ "Tangent", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            Spieler::InputLayoutElement{ "TexCoord", DXGI_FORMAT_R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
        };

        Spieler::PipelineStateProps pipelineStateProps;
        pipelineStateProps.RootSignature = &m_RootSignatures["default"];
        pipelineStateProps.VertexShader = &m_VertexShaders["texture"];
        pipelineStateProps.PixelShader = &m_PixelShaders["texture"];
        pipelineStateProps.RasterizerState = &rasterzerState;
        pipelineStateProps.InputLayout = &inputLayout;
        pipelineStateProps.PrimitiveTopologyType = Spieler::PrimitiveTopologyType_Triangle;
        pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
        pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

        SPIELER_RETURN_IF_FAILED(m_PipelineStates["default"].Init(pipelineStateProps));

        return true;
    }

    void TestLayer::InitConstantBuffers()
    {
        m_ConstantBuffers["pass"].InitAsRootDescriptor(&m_UploadBuffers["pass"], 0);
    }

    void TestLayer::UpdateViewport()
    {
        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(m_Window.GetWidth());
        m_Viewport.Height = static_cast<float>(m_Window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TestLayer::UpdateScissorRect()
    {
        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(m_Window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(m_Window.GetHeight());
    }

    bool TestLayer::OnWindowResized(Spieler::WindowResizedEvent& event)
    {
        UpdateViewport();
        UpdateScissorRect();

        return true;
    }

} // namespace Sandbox