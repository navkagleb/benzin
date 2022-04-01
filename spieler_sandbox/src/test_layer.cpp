#include "test_layer.hpp"

#include <spieler/window/event.hpp>
#include <spieler/window/event_dispatcher.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/rasterizer_state.hpp>

namespace Sandbox
{

    TestLayer::TestLayer(Spieler::Window& window, Spieler::Renderer& renderer)
        : m_Window(window)
        , m_Renderer(renderer)
        , m_CameraController(DirectX::XM_PIDIV2, m_Window.GetAspectRatio())
    {}

    bool TestLayer::OnAttach()
    {
        Spieler::UploadBuffer textureUploadBuffer;
        Spieler::UploadBuffer vertexUploadBuffer;
        Spieler::UploadBuffer indexUploadBuffer;

        UpdateViewport();
        UpdateScissorRect();

        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitUploadBuffers());

        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures(textureUploadBuffer));
            InitMaterials();
            SPIELER_RETURN_IF_FAILED(InitMeshGeometries(vertexUploadBuffer, indexUploadBuffer));
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList());

        SPIELER_RETURN_IF_FAILED(InitRootSignatures());
        SPIELER_RETURN_IF_FAILED(InitPipelineStates());

        InitConstantBuffers();
        InitLights();

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
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineStates["lit"]));
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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorCPUHandle(currentBackBuffer) };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorCPUHandle(0) };

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_RootSignatures["default"].Bind();
            m_ConstantBuffers["pass"].Bind(0);

            m_DescriptorHeaps["srv"].Bind();

            for (const auto& [renderItemName, renderItem] : m_LitRenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                m_VertexBufferViews["basic_meshes"].Bind();
                m_IndexBufferViews["basic_meshes"].Bind();
                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->PrimitiveTopology));

                renderItem.ConstantBuffer.Bind(1);

                if (renderItem.Material)
                {
                    renderItem.Material->ConstantBuffer.Bind(2);
                    renderItem.Material->DiffuseMap.Bind(3);
                }
                
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

        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineStates["unlit"]));
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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorCPUHandle(currentBackBuffer) };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorCPUHandle(0) };

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_RootSignatures["default"].Bind();
            m_ConstantBuffers["pass"].Bind(0);

            for (const auto& [renderItemName, renderItem] : m_UnlitRenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                m_VertexBufferViews["basic_meshes"].Bind();
                m_IndexBufferViews["basic_meshes"].Bind();
                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->PrimitiveTopology));

                renderItem.ConstantBuffer.Bind(1);

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
        m_DirectionalLightController.OnImGuiRender();
    }

    bool TestLayer::InitDescriptorHeaps()
    {
        SPIELER_RETURN_IF_FAILED(m_DescriptorHeaps["srv"].Init(Spieler::DescriptorHeapType::SRV, 1));

        return true;
    }

    bool TestLayer::InitUploadBuffers()
    {
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["pass"].Init<PassConstants>(Spieler::UploadBufferType::ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["lit_object"].Init<LitObjectConstants>(Spieler::UploadBufferType::ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["unlit_object"].Init<UnlitObjectConstants>(Spieler::UploadBufferType::ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["material"].Init<Spieler::MaterialConstants>(Spieler::UploadBufferType::ConstantBuffer, 1));

        return true;
    }

    bool TestLayer::InitTextures(Spieler::UploadBuffer& textureUploadBuffer)
    {
        SPIELER_RETURN_IF_FAILED(m_Textures["wood_crate1"].LoadFromDDSFile(L"assets/textures/wood_crate1.dds", textureUploadBuffer));
        
        return true;
    }

    void TestLayer::InitMaterials()
    {
        auto& woodMaterial{ m_Materials["wood_crate1"] };
        woodMaterial.DiffuseMap.Init(m_Textures["wood_crate1"], m_DescriptorHeaps["srv"], 0);
        woodMaterial.ConstantBuffer.Init(m_UploadBuffers["material"], 0);

        auto& woodMaterialConstants{ woodMaterial.ConstantBuffer.As<Spieler::MaterialConstants>() };
        woodMaterialConstants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        woodMaterialConstants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
        woodMaterialConstants.Roughness = 0.2f;
        woodMaterialConstants.Transform = DirectX::XMMatrixIdentity();
    }

    bool TestLayer::InitMeshGeometries(Spieler::UploadBuffer& vertexUploadBuffer, Spieler::UploadBuffer& indexUploadBuffer)
    {
        Spieler::BoxGeometryProps boxGeometryProps;
        boxGeometryProps.Width = 10.0f;
        boxGeometryProps.Height = 10.0f;
        boxGeometryProps.Depth = 10.0f;

        Spieler::GeosphereGeometryProps geosphereGeometryProps;
        geosphereGeometryProps.Radius = 10.0f;
        geosphereGeometryProps.SubdivisionCount = 2;

        const auto& boxMeshData{ Spieler::GeometryGenerator::GenerateBox<Spieler::BasicVertex, std::uint32_t>(boxGeometryProps) };
        const auto& geosphereMeshData{ Spieler::GeometryGenerator::GenerateGeosphere<Spieler::BasicVertex, std::uint32_t>(geosphereGeometryProps) };

        Spieler::MeshGeometry& basicMeshesGeometry{ m_MeshGeometries["basic_meshes"] };

        Spieler::SubmeshGeometry& boxSubmeshGeometry{ basicMeshesGeometry.Submeshes["box"] };
        boxSubmeshGeometry.IndexCount = static_cast<std::uint32_t>(boxMeshData.Indices.size());
        boxSubmeshGeometry.BaseVertexLocation = 0;
        boxSubmeshGeometry.StartIndexLocation = 0;

        Spieler::SubmeshGeometry& geosphereSubmeshGeometry{ basicMeshesGeometry.Submeshes["geosphere"] };
        geosphereSubmeshGeometry.IndexCount = static_cast<std::uint32_t>(geosphereMeshData.Indices.size());
        geosphereSubmeshGeometry.BaseVertexLocation = static_cast<std::uint32_t>(boxMeshData.Vertices.size());
        geosphereSubmeshGeometry.StartIndexLocation = static_cast<std::uint32_t>(boxMeshData.Indices.size());

        const auto vertexCount{ boxMeshData.Vertices.size() + geosphereMeshData.Vertices.size() };
        const auto indexCount{ boxMeshData.Indices.size() + geosphereMeshData.Indices.size() };

        std::vector<Spieler::BasicVertex> vertices;
        vertices.reserve(vertexCount);
        vertices.insert(vertices.end(), boxMeshData.Vertices.begin(), boxMeshData.Vertices.end());
        vertices.insert(vertices.end(), geosphereMeshData.Vertices.begin(), geosphereMeshData.Vertices.end());

        std::vector<std::uint32_t> indices;
        indices.reserve(indexCount);
        indices.insert(indices.end(), boxMeshData.Indices.begin(), boxMeshData.Indices.end());
        indices.insert(indices.end(), geosphereMeshData.Indices.begin(), geosphereMeshData.Indices.end());

        basicMeshesGeometry.PrimitiveTopology = Spieler::PrimitiveTopology::TriangleList;
        SPIELER_RETURN_IF_FAILED(basicMeshesGeometry.VertexBuffer.Init(vertices.data(), static_cast<std::uint32_t>(vertices.size()), vertexUploadBuffer));
        SPIELER_RETURN_IF_FAILED(basicMeshesGeometry.IndexBuffer.Init(indices.data(), static_cast<std::uint32_t>(indices.size()), indexUploadBuffer));

        m_VertexBufferViews["basic_meshes"].Init(basicMeshesGeometry.VertexBuffer);
        m_IndexBufferViews["basic_meshes"].Init(basicMeshesGeometry.IndexBuffer);

        // Render items
        Spieler::RenderItem& boxRenderItem{ m_LitRenderItems["box"] };
        boxRenderItem.MeshGeometry = &basicMeshesGeometry;
        boxRenderItem.SubmeshGeometry = &boxSubmeshGeometry;
        boxRenderItem.Material = &m_Materials["wood_crate1"]; 
        boxRenderItem.ConstantBuffer.Init(m_UploadBuffers["lit_object"], 0);

        auto& boxConstants{ boxRenderItem.ConstantBuffer.As<LitObjectConstants>() };
        boxConstants.World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0, 0.0f, 20.0f));
        boxConstants.TextureTransform = DirectX::XMMatrixIdentity();

        Spieler::RenderItem& geosphereRenderItem{ m_UnlitRenderItems["geosphere"] };
        geosphereRenderItem.MeshGeometry = &basicMeshesGeometry;
        geosphereRenderItem.SubmeshGeometry = &geosphereSubmeshGeometry;
        geosphereRenderItem.ConstantBuffer.Init(m_UploadBuffers["unlit_object"], 0);
        geosphereRenderItem.ConstantBuffer.As<UnlitObjectConstants>().Color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 0.2f, 1.0f };

        return true;
    }

    bool TestLayer::InitRootSignatures()
    {
        std::vector<Spieler::RootParameter> rootParameters;
        std::vector<Spieler::StaticSampler> staticSamplers;

        // Init root parameters
        {
            rootParameters.resize(4);

            rootParameters[0].Type = Spieler::RootParameterType_ConstantBufferView;
            rootParameters[0].ShaderVisibility = Spieler::ShaderVisibility_All;
            rootParameters[0].Child = Spieler::RootDescriptor{ 0, 0 };

            rootParameters[1].Type = Spieler::RootParameterType_ConstantBufferView;
            rootParameters[1].ShaderVisibility = Spieler::ShaderVisibility_All;
            rootParameters[1].Child = Spieler::RootDescriptor{ 1, 0 };

            rootParameters[2].Type = Spieler::RootParameterType_ConstantBufferView;
            rootParameters[2].ShaderVisibility = Spieler::ShaderVisibility_All;
            rootParameters[2].Child = Spieler::RootDescriptor{ 2, 0 };

            rootParameters[3].Type = Spieler::RootParameterType_DescriptorTable;
            rootParameters[3].ShaderVisibility = Spieler::ShaderVisibility_All;
            rootParameters[3].Child = Spieler::RootDescriptorTable{ { Spieler::DescriptorRange{ Spieler::DescriptorRangeType_SRV, 1 } } };
        }

        // Init statis samplers
        {
            staticSamplers.resize(6);

            staticSamplers[0] = Spieler::StaticSampler(Spieler::TextureFilterType_Point, Spieler::TextureAddressMode_Wrap, 0);
            staticSamplers[1] = Spieler::StaticSampler(Spieler::TextureFilterType_Point, Spieler::TextureAddressMode_Clamp, 1);
            staticSamplers[2] = Spieler::StaticSampler(Spieler::TextureFilterType_Linear, Spieler::TextureAddressMode_Wrap, 2);
            staticSamplers[3] = Spieler::StaticSampler(Spieler::TextureFilterType_Linear, Spieler::TextureAddressMode_Clamp, 3);
            staticSamplers[4] = Spieler::StaticSampler(Spieler::TextureFilterType_Anisotropic, Spieler::TextureAddressMode_Wrap, 4);
            staticSamplers[5] = Spieler::StaticSampler(Spieler::TextureFilterType_Anisotropic, Spieler::TextureAddressMode_Clamp, 5);
        }

        SPIELER_RETURN_IF_FAILED(m_RootSignatures["default"].Init(rootParameters, staticSamplers));

        return true;
    }

    bool TestLayer::InitPipelineStates()
    {
        // Lit PSO
        {
            std::vector<Spieler::ShaderDefine> defines
            {
                Spieler::ShaderDefine{ "DIRECTIONAL_LIGHT_COUNT", "1" }
            };

            Spieler::VertexShader& vertexShader{ m_VertexShaders["texture"] };
            Spieler::PixelShader& pixelShader{ m_PixelShaders["texture"] };

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/texture.fx", "VS_Main"));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/texture.fx", "PS_Main", defines));

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
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.RasterizerState = &rasterzerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = Spieler::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["lit"].Init(pipelineStateProps));
        }
        

        // Unlit PSO
        {
            Spieler::VertexShader& vertexShader{ m_VertexShaders["color"] };
            Spieler::PixelShader& pixelShader{ m_PixelShaders["color"] };

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/color2.fx", "VS_Main"));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/color2.fx", "PS_Main"));

            Spieler::RasterizerState rasterzerState;
            rasterzerState.FillMode = Spieler::FillMode_Wireframe;
            rasterzerState.CullMode = Spieler::CullMode_None;

            Spieler::InputLayout inputLayout
            {
                Spieler::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "Normal", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "Tangent", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "TexCoord", DXGI_FORMAT_R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
            };

            Spieler::PipelineStateProps pipelineStateProps;
            pipelineStateProps.RootSignature = &m_RootSignatures["default"];
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.RasterizerState = &rasterzerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = Spieler::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["unlit"].Init(pipelineStateProps));
        }

        return true;
    }

    void TestLayer::InitConstantBuffers()
    {
        m_ConstantBuffers["pass"].Init(m_UploadBuffers["pass"], 0);
        m_ConstantBuffers["pass"].As<PassConstants>().AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
    }

    void TestLayer::InitLights()
    {
        Spieler::LightConstants constants;
        constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };

        m_DirectionalLightController.SetConstants(&m_UploadBuffers["pass"].As<PassConstants>(0).Lights[0]);
        m_DirectionalLightController.SetShape(&m_UnlitRenderItems["geosphere"]);
        m_DirectionalLightController.Init(constants, DirectX::XM_PIDIV2 * 1.25f, DirectX::XM_PIDIV4);
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