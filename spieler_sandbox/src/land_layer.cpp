#include "land_layer.hpp"

#include <random>
#include <chrono>

#include <DirectXColors.h>

#include <imgui/imgui.h>

#include <spieler/application.hpp>
#include <spieler/common.hpp>
#include <spieler/math.hpp>
#include <spieler/window/input.hpp>
#include <spieler/window/event_dispatcher.hpp>
#include <spieler/window/window_event.hpp>
#include <spieler/window/mouse_event.hpp>
#include <spieler/window/key_event.hpp>
#include <spieler/utility/random.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/sampler.hpp>
#include <spieler/renderer/rasterizer_state.hpp>

namespace Sandbox
{

    constexpr std::uint32_t MAX_LIGHT_COUNT{ 16 };

    struct PassConstants
    {
        alignas(16) DirectX::XMMATRIX View{};
        alignas(16) DirectX::XMMATRIX Projection{};
        alignas(16) DirectX::XMFLOAT3 CameraPosition{};
        alignas(16) DirectX::XMFLOAT4 AmbientLight{};
        alignas(16) Spieler::LightConstants Lights[MAX_LIGHT_COUNT];
        alignas(16) DirectX::XMFLOAT4 FogColor;
        float FogStart;
        float FogRange;
    };

    struct ObjectConstants
    {
        DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX TextureTransform{ DirectX::XMMatrixIdentity() };
    };

    namespace _Internal
    {

        static void SetLandHeight(DirectX::XMFLOAT3& position)
        {
            position.y = 0.3f * (position.z * std::sin(0.1f * position.x) + position.x * std::cos(0.1f * position.z));
        }

        static void SetLandNormal(const DirectX::XMFLOAT3& position, DirectX::XMFLOAT3& normal)
        {
            normal.x = -0.03f * position.z * cosf(0.1f * position.x) - 0.3f * cosf(0.1f * position.z);
            normal.y = 1.0f;
            normal.z = -0.3f * sinf(0.1f * position.x) + 0.03f * position.x * sinf(0.1f * position.z);

            const DirectX::XMVECTOR unitNormal{ DirectX::XMVector3Normalize(DirectX::XMLoadFloat3(&normal)) };
            DirectX::XMStoreFloat3(&normal, unitNormal);
        }

    } // namespace _Internal

    LandLayer::LandLayer(Spieler::Window& window, Spieler::Renderer& renderer)
        : m_Window(window)
        , m_Renderer(renderer)
        , m_CameraController(DirectX::XM_PIDIV2, m_Window.GetAspectRatio())
        , m_Waves(WavesProps{ 400, 400, 0.03f, 0.4f, 4.0f, 0.2f })
    {}

    bool LandLayer::OnAttach()
    {
        Spieler::UploadBuffer grassTextureUploadBuffer;
        Spieler::UploadBuffer waterTextreUploadBuffer;
        MeshUploadBuffer meshUploadBuffer;
        MeshUploadBuffer landUploadBuffer;
        MeshUploadBuffer wavesUploadBuffer;

        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitUploadBuffers());

        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures(grassTextureUploadBuffer, waterTextreUploadBuffer));

            InitMaterials();

            SPIELER_RETURN_IF_FAILED(InitMeshGeometry(meshUploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitLandGeometry(landUploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitWavesGeometry(wavesUploadBuffer));
        }
        SPIELER_RETURN_IF_FAILED(m_Renderer.ExexuteCommandList());
        
        SPIELER_RETURN_IF_FAILED(InitRootSignature());
        SPIELER_RETURN_IF_FAILED(InitPipelineState());

        InitPassConstantBuffer();
        InitLightControllers();
        InitViewport();
        InitScissorRect();

        return true;
    }

    void LandLayer::OnEvent(Spieler::Event& event)
    {
        Spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        m_CameraController.OnEvent(event);
    }

    void LandLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Update m_PassConstantBuffer
        const auto& camera{ m_CameraController.GetCamera() };

        auto& passConstants{ m_PassConstantBuffer.As<PassConstants>() };
        passConstants.View = DirectX::XMMatrixTranspose(camera.View);
        passConstants.Projection = DirectX::XMMatrixTranspose(camera.Projection);
        DirectX::XMStoreFloat3(&passConstants.CameraPosition, camera.EyePosition);

        // Update m_Waves
        static float time = 0.0f;
        
        if ((Spieler::Application::GetInstance().GetTimer().GetTotalTime() - time) >= 0.25f)
        {
            time += 0.25f;

            const auto i = Spieler::Random::GetInstance().GetIntegral<std::uint32_t>(4, m_Waves.GetRowCount() - 5);
            const auto j = Spieler::Random::GetInstance().GetIntegral<std::uint32_t>(4, m_Waves.GetColumnCount() - 5);
            const auto r = Spieler::Random::GetInstance().GetFloatingPoint<float>(0.2f, 0.5f);

            m_Waves.Disturb(i, j, r);
        }

        m_Waves.OnUpdate(dt);

        UpdateWavesVertexBuffer(dt);
    }

    bool LandLayer::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(RenderLitRenderItems());
        SPIELER_RETURN_IF_FAILED(RenderColorRenderItems());

        return true;
    }

    void LandLayer::OnImGuiRender(float dt)
    {
        ImGui::Begin("Settings");
        {
            ImGui::ColorEdit3("Clear color", reinterpret_cast<float*>(&m_ClearColor));
            ImGui::Separator();
            ImGui::Text("FPS: %.1f, ms: %.3f", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
        }
        ImGui::End();

        m_CameraController.OnImGuiRender(dt);
        m_DirectionalLightController.OnImGuiRender();
        m_PointLightController.OnImGuiRender();
        m_SpotLightController.OnImGuiRender();
    }

    bool LandLayer::InitDescriptorHeaps()
    {
        SPIELER_RETURN_IF_FAILED(m_DescriptorHeaps["srv"].Init(Spieler::DescriptorHeapType::SRV, 2));

        return true;
    }

    bool LandLayer::InitUploadBuffers()
    {
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["pass"].Init<PassConstants>(Spieler::UploadBufferType::ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["object"].Init<ObjectConstants>(Spieler::UploadBufferType::ConstantBuffer, m_RenderItemCount));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["waves"].Init<Vertex>(Spieler::UploadBufferType::Default, m_Waves.GetVertexCount()));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["material"].Init<Spieler::MaterialConstants>(Spieler::UploadBufferType::ConstantBuffer, m_MaterialCount));

        return true;
    }

    bool LandLayer::InitTextures(Spieler::UploadBuffer& grassTextureUploadBuffer, Spieler::UploadBuffer& waterTextureUploadBuffer)
    {
        SPIELER_RETURN_IF_FAILED(m_Textures["grass"].LoadFromDDSFile(L"assets/textures/grass.dds", grassTextureUploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["water"].LoadFromDDSFile(L"assets/textures/water.dds", waterTextureUploadBuffer));

        return true;
    }

    void LandLayer::InitMaterials()
    {
        auto& materialUploadBuffer{ m_UploadBuffers["material"] };
        const auto& descriptorHeap{ m_DescriptorHeaps["srv"] };

        // Grass material
        {
            Spieler::Material& grass{ m_Materials["grass"] };
            grass.DiffuseMap.Init(m_Textures["grass"], descriptorHeap, 0);
            grass.ConstantBuffer.Init(materialUploadBuffer, 0);

            Spieler::MaterialConstants& grassConstants{ grass.ConstantBuffer.As<Spieler::MaterialConstants>() };
            grassConstants.DiffuseAlbedo = DirectX::XMFLOAT4{ 0.2f, 0.6f, 0.2f, 1.0f };
            grassConstants.FresnelR0 = DirectX::XMFLOAT3{ 0.01f, 0.01f, 0.01f };
            grassConstants.Roughness = 0.9f;
            grassConstants.Transform = DirectX::XMMatrixIdentity();
        }

        // Water material
        {
            Spieler::Material& water{ m_Materials["water"] };
            water.DiffuseMap.Init(m_Textures["water"], descriptorHeap, 1);
            water.ConstantBuffer.Init(materialUploadBuffer, 1);

            Spieler::MaterialConstants& waterConstants{ water.ConstantBuffer.As<Spieler::MaterialConstants>() };
            waterConstants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            waterConstants.FresnelR0 = DirectX::XMFLOAT3{ 0.2f, 0.2f, 0.2f };
            waterConstants.Roughness = 0.0f;
            waterConstants.Transform = DirectX::XMMatrixIdentity();
        }
    }

    bool LandLayer::InitMeshGeometry(MeshUploadBuffer& meshUploadBuffers)
    {
        Spieler::BoxGeometryProps boxProps;
        boxProps.Width = 30.0f;
        boxProps.Height = 20.0f;
        boxProps.Depth = 25.0f;
        boxProps.SubdivisionCount = 2;

        Spieler::GridGeometryProps gridProps;
        gridProps.Width = 100.0f;
        gridProps.Depth = 100.0f;
        gridProps.RowCount = 10;
        gridProps.ColumnCount = 10;

        Spieler::CylinderGeometryProps cylinderProps;
        cylinderProps.TopRadius = 10.0f;
        cylinderProps.BottomRadius = 20.0f;
        cylinderProps.Height = 40.0f;
        cylinderProps.SliceCount = 20;
        cylinderProps.StackCount = 3;

        Spieler::SphereGeometryProps sphereProps;
        sphereProps.Radius = 20.0f;
        sphereProps.SliceCount = 30;
        sphereProps.StackCount = 10;

        Spieler::GeosphereGeometryProps geosphereProps;
        geosphereProps.Radius = 20.0f;
        geosphereProps.SubdivisionCount = 2;

        const Spieler::MeshData boxMeshData = Spieler::GeometryGenerator::GenerateBox<Spieler::BasicVertex, std::uint32_t>(boxProps);
        const Spieler::MeshData gridMeshData = Spieler::GeometryGenerator::GenerateGrid<Spieler::BasicVertex, std::uint32_t>(gridProps);
        const Spieler::MeshData cylinderMeshData = Spieler::GeometryGenerator::GenerateCylinder<Spieler::BasicVertex, std::uint32_t>(cylinderProps);
        const Spieler::MeshData sphereMeshData = Spieler::GeometryGenerator::GenerateSphere<Spieler::BasicVertex, std::uint32_t>(sphereProps);
        const Spieler::MeshData geosphereMeshData = Spieler::GeometryGenerator::GenerateGeosphere<Spieler::BasicVertex, std::uint32_t>(geosphereProps);

        auto& meshes{ m_MeshGeometries["meshes"] };

        Spieler::SubmeshGeometry& boxSubmesh{ meshes.Submeshes["box"] };
        boxSubmesh.IndexCount = static_cast<std::uint32_t>(boxMeshData.Indices.size());
        boxSubmesh.StartIndexLocation = 0;
        boxSubmesh.BaseVertexLocation = 0;

        Spieler::SubmeshGeometry& gridSubmesh{ meshes.Submeshes["grid"] };
        gridSubmesh.IndexCount = static_cast<std::uint32_t>(gridMeshData.Indices.size());
        gridSubmesh.BaseVertexLocation = boxSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(boxMeshData.Vertices.size());
        gridSubmesh.StartIndexLocation = boxSubmesh.StartIndexLocation + static_cast<std::uint32_t>(boxMeshData.Indices.size());

        Spieler::SubmeshGeometry& cylinderSubmesh{ meshes.Submeshes["cylinder"] };
        cylinderSubmesh.IndexCount = static_cast<std::uint32_t>(cylinderMeshData.Indices.size());
        cylinderSubmesh.BaseVertexLocation = gridSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(gridMeshData.Vertices.size());
        cylinderSubmesh.StartIndexLocation = gridSubmesh.StartIndexLocation + static_cast<std::uint32_t>(gridMeshData.Indices.size());

        Spieler::SubmeshGeometry& sphereSubmesh{ meshes.Submeshes["sphere"] };
        sphereSubmesh.IndexCount = static_cast<std::uint32_t>(sphereMeshData.Indices.size());
        sphereSubmesh.BaseVertexLocation = cylinderSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(cylinderMeshData.Vertices.size());
        sphereSubmesh.StartIndexLocation = cylinderSubmesh.StartIndexLocation + static_cast<std::uint32_t>(cylinderMeshData.Indices.size());

        Spieler::SubmeshGeometry& geosphereSubmesh{ meshes.Submeshes["geosphere"] };
        geosphereSubmesh.IndexCount = static_cast<std::uint32_t>(geosphereMeshData.Indices.size());
        geosphereSubmesh.BaseVertexLocation = sphereSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(sphereMeshData.Vertices.size());
        geosphereSubmesh.StartIndexLocation = sphereSubmesh.StartIndexLocation + static_cast<std::uint32_t>(sphereMeshData.Indices.size());

        const std::uint32_t vertexCount = boxMeshData.Vertices.size() + gridMeshData.Vertices.size() + cylinderMeshData.Vertices.size() +  sphereMeshData.Vertices.size() + geosphereMeshData.Vertices.size();
        const std::uint32_t indexCount = boxMeshData.Indices.size() + gridMeshData.Indices.size() + cylinderMeshData.Indices.size() + sphereMeshData.Indices.size() + geosphereMeshData.Indices.size();

        std::vector<ColorVertex> vertices;
        vertices.reserve(vertexCount);

        for (const auto& vertex : boxMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = vertex.Position;
            DirectX::XMStoreFloat4(&colorVertex.Color, DirectX::Colors::OrangeRed);

            vertices.push_back(colorVertex);
        }

        for (const auto& vertex : gridMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = vertex.Position;
            DirectX::XMStoreFloat4(&colorVertex.Color, DirectX::Colors::Bisque);

            vertices.push_back(colorVertex);
        }

        for (const auto& vertex : cylinderMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = vertex.Position;
            DirectX::XMStoreFloat4(&colorVertex.Color, DirectX::Colors::Coral);

            vertices.push_back(colorVertex);
        }

        for (const auto& vertex : sphereMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = vertex.Position;
            DirectX::XMStoreFloat4(&colorVertex.Color, DirectX::Colors::LawnGreen);

            vertices.push_back(colorVertex);
        }

        for (const auto& vertex : geosphereMeshData.Vertices)
        {
            ColorVertex colorVertex;
            colorVertex.Position = vertex.Position;
            DirectX::XMStoreFloat4(&colorVertex.Color, DirectX::Colors::WhiteSmoke);

            vertices.push_back(colorVertex);
        }

        // Indices
        std::vector<std::uint32_t> indices;
        indices.reserve(indexCount);
        indices.insert(indices.end(), boxMeshData.Indices.begin(),       boxMeshData.Indices.end());
        indices.insert(indices.end(), gridMeshData.Indices.begin(),      gridMeshData.Indices.end());
        indices.insert(indices.end(), cylinderMeshData.Indices.begin(),  cylinderMeshData.Indices.end());
        indices.insert(indices.end(), sphereMeshData.Indices.begin(),    sphereMeshData.Indices.end());
        indices.insert(indices.end(), geosphereMeshData.Indices.begin(), geosphereMeshData.Indices.end());

        SPIELER_RETURN_IF_FAILED(meshes.VertexBuffer.Init(vertices.data(), static_cast<std::uint32_t>(vertices.size()), meshUploadBuffers.VertexUploadBuffer));
        SPIELER_RETURN_IF_FAILED(meshes.IndexBuffer.Init(indices.data(), static_cast<std::uint32_t>(indices.size()), meshUploadBuffers.IndexUploadBuffer));

        Spieler::VertexBufferView vbv(meshes.VertexBuffer);
        Spieler::IndexBufferView ibv(meshes.IndexBuffer);

        meshes.PrimitiveTopology = Spieler::PrimitiveTopology::TriangleList;

#if 0
        // Render items
        Spieler::RenderItem box;
        box.MeshGeometry        = &meshes;
        box.SubmeshGeometry     = &meshes.Submeshes["box"];
        
        SPIELER_RETURN_IF_FAILED(box.ConstantBuffer.InitAsRootDescriptor(&m_ObjectUploadBuffer, 0));
        m_ColorRenderItems.push_back(std::move(box));
#endif

#if 0
        Spieler::RenderItem sphere;
        sphere.MeshGeometry           = &meshes;
        sphere.SubmeshGeometry        = &meshes.Submeshes["sphere"];

        SPIELER_RETURN_IF_FAILED(sphere.ConstantBuffer.InitAsRootDescriptor(&m_ObjectUploadBuffer, 2));
        m_ColorRenderItems.push_back(std::move(sphere));
#endif

        // For m_DirectionalLight
        {
            const std::string renderItemName{ "geosphere1" };

            Spieler::RenderItem& geosphere{ m_ColorRenderItems[renderItemName] };
            geosphere.MeshGeometry = &meshes;
            geosphere.SubmeshGeometry = &meshes.Submeshes["geosphere"];
            geosphere.ConstantBuffer.Init(m_UploadBuffers["object"], 2);

            ObjectConstants& geosphereConstants{ geosphere.ConstantBuffer.As<ObjectConstants>() };
            geosphereConstants.World = DirectX::XMMatrixIdentity();
            geosphereConstants.TextureTransform = DirectX::XMMatrixIdentity();

            m_VertexBufferViews[renderItemName] = vbv;
            m_IndexBufferViews[renderItemName] = ibv;
        }

        // For m_PointLight
        {
            const std::string renderItemName{ "geosphere2" };

            Spieler::RenderItem& geosphere{ m_ColorRenderItems[renderItemName]};
            geosphere.MeshGeometry = &meshes;
            geosphere.SubmeshGeometry = &meshes.Submeshes["geosphere"];
            geosphere.ConstantBuffer.Init(m_UploadBuffers["object"], 3);

            ObjectConstants& geosphereConstants{ geosphere.ConstantBuffer.As<ObjectConstants>() };
            geosphereConstants.World = DirectX::XMMatrixIdentity();
            geosphereConstants.TextureTransform = DirectX::XMMatrixIdentity();

            m_VertexBufferViews[renderItemName] = vbv;
            m_IndexBufferViews[renderItemName] = ibv;
        }

        // For m_SpotLight
        {
            const std::string renderItemName{ "cylinder" };

            Spieler::RenderItem& cylinder{ m_ColorRenderItems["cylinder"] };
            cylinder.MeshGeometry = &meshes;
            cylinder.SubmeshGeometry = &meshes.Submeshes["cylinder"];
            cylinder.ConstantBuffer.Init(m_UploadBuffers["object"], 4);

            ObjectConstants& cylinderConstants{ cylinder.ConstantBuffer.As<ObjectConstants>() };
            cylinderConstants.World = DirectX::XMMatrixIdentity();
            cylinderConstants.TextureTransform = DirectX::XMMatrixIdentity();

            m_VertexBufferViews[renderItemName] = vbv;
            m_IndexBufferViews[renderItemName] = ibv;
        }

        return true;
    }

    bool LandLayer::InitLandGeometry(MeshUploadBuffer& landUploadBuffer)
    {
        Spieler::GridGeometryProps gridGeometryProps;
        gridGeometryProps.Width = 160.0f;
        gridGeometryProps.Depth = 160.0f;
        gridGeometryProps.RowCount = 100;
        gridGeometryProps.ColumnCount = 100;

        const auto& gridMeshData = Spieler::GeometryGenerator::GenerateGrid<Spieler::BasicVertex, std::uint32_t>(gridGeometryProps);

        std::vector<Vertex> vertices;
        vertices.reserve(gridMeshData.Vertices.size());

        for (const auto& basicVertex : gridMeshData.Vertices)
        {
            Vertex vertex;
            vertex.Position = basicVertex.Position;
            vertex.TexCoord = basicVertex.TexCoord;

            _Internal::SetLandHeight(vertex.Position);
            _Internal::SetLandNormal(vertex.Position, vertex.Normal);


#if 0
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
#endif

            vertices.push_back(vertex);
        }

        auto& landMeshGeometry = m_MeshGeometries["land"];

        SPIELER_RETURN_IF_FAILED(landMeshGeometry.VertexBuffer.Init(vertices.data(), vertices.size(), landUploadBuffer.VertexUploadBuffer));
        SPIELER_RETURN_IF_FAILED(landMeshGeometry.IndexBuffer.Init(gridMeshData.Indices.data(), gridMeshData.Indices.size(), landUploadBuffer.IndexUploadBuffer));

        m_VertexBufferViews["land"].Init(landMeshGeometry.VertexBuffer);
        m_IndexBufferViews["land"].Init(landMeshGeometry.IndexBuffer);

        landMeshGeometry.PrimitiveTopology = Spieler::PrimitiveTopology::TriangleList;

        auto& landMesh = landMeshGeometry.Submeshes["land"];
        landMesh.IndexCount = gridMeshData.Indices.size();

        // Render items
        Spieler::RenderItem& land{ m_LitRenderItems["land"] };
        land.MeshGeometry = &landMeshGeometry;
        land.SubmeshGeometry = &landMeshGeometry.Submeshes["land"];
        land.Material = &m_Materials["grass"];
        land.ConstantBuffer.Init(m_UploadBuffers["object"], 0);

        ObjectConstants& landConstants{ land.ConstantBuffer.As<ObjectConstants>() };
        landConstants.World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -20.0f, 0.0f));
        landConstants.TextureTransform = DirectX::XMMatrixScaling(5.0f, 5.0f, 1.0f);

        return true;
    }

    bool LandLayer::InitWavesGeometry(MeshUploadBuffer& wavesUploadBuffer)
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

        SPIELER_RETURN_IF_FAILED(wavesMeshGeometry.IndexBuffer.Init(indices.data(), static_cast<std::uint32_t>(indices.size()), wavesUploadBuffer.IndexUploadBuffer));

        m_IndexBufferViews["waves"].Init(wavesMeshGeometry.IndexBuffer);
        m_VertexBufferViews["waves"].Init(m_UploadBuffers["waves"]);

        wavesMeshGeometry.PrimitiveTopology = Spieler::PrimitiveTopology::TriangleList;

        auto& submesh{ wavesMeshGeometry.Submeshes["main"] };
        submesh.IndexCount = static_cast<std::uint32_t>(indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;

        // Render Item
        Spieler::RenderItem& waves{ m_LitRenderItems["waves"] };
        waves.MeshGeometry = &wavesMeshGeometry;
        waves.SubmeshGeometry = &wavesMeshGeometry.Submeshes["main"];
        waves.Material = &m_Materials["water"];

        waves.ConstantBuffer.Init(m_UploadBuffers["object"], 1);
        waves.ConstantBuffer.As<ObjectConstants>().World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -25.0f, 0.0f));
        waves.ConstantBuffer.As<ObjectConstants>().TextureTransform = DirectX::XMMatrixScaling(10.0f, 10.0f, 1.0f);

        return true;
    }

    bool LandLayer::InitRootSignature()
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

    bool LandLayer::InitPipelineState()
    {
        // Lit PSO
        {
            auto& vertexShader = m_VertexShaders["default"];
            auto& pixelShader = m_PixelShaders["default"];

            const std::vector<Spieler::ShaderDefine> defines
            {
                Spieler::ShaderDefine{ "DIRECTIONAL_LIGHT_COUNT", "1" },
                Spieler::ShaderDefine{ "POINT_LIGHT_COUNT", "1" },
                Spieler::ShaderDefine{ "SPOT_LIGHT_COUNT", "1" },
            };

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/default.fx", "VS_Main", defines));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/default.fx", "PS_Main", defines));

            Spieler::RasterizerState rasterizerState;
            rasterizerState.FillMode = Spieler::FillMode::Solid;
            rasterizerState.CullMode = Spieler::CullMode::Back;

            Spieler::InputLayout inputLayout
            {
                Spieler::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "Normal", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "TexCoord", DXGI_FORMAT_R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
            };

            Spieler::PipelineStateProps pipelineStateProps;
            pipelineStateProps.RootSignature = &m_RootSignatures["default"];
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.RasterizerState = &rasterizerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = Spieler::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["lit"].Init(pipelineStateProps));
        }

        // Color PSO
        {
            auto& vertexShader = m_VertexShaders["color"];
            auto& pixelShader = m_PixelShaders["color"];

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/color1.fx", "VS_Main"));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/color1.fx", "PS_Main"));

            Spieler::RasterizerState rasterzerState;
            rasterzerState.FillMode = Spieler::FillMode::Wireframe;
            rasterzerState.CullMode = Spieler::CullMode::None;

            Spieler::InputLayout inputLayout
            {
                Spieler::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                Spieler::InputLayoutElement{ "Color", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT4) }
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

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["color"].Init(pipelineStateProps));
        }

        return true;
    }

    void LandLayer::InitPassConstantBuffer()
    {
        m_PassConstantBuffer.Init(m_UploadBuffers["pass"], 0);

        auto& passConstants{ m_PassConstantBuffer.As<PassConstants>() };
        passConstants.AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
        passConstants.FogColor = m_ClearColor;
        passConstants.FogStart = 30.0f;
        passConstants.FogRange = 100.0f;
    }

    void LandLayer::InitLightControllers()
    {
        auto& lightsConstants = m_PassConstantBuffer.As<PassConstants>().Lights;

        // Directional light
        {
            Spieler::LightConstants constants;
            constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.7f };

            m_DirectionalLightController.SetConstants(&lightsConstants[0]);
            m_DirectionalLightController.SetShape(&m_ColorRenderItems["geosphere1"]);
            m_DirectionalLightController.SetName("Sun Controller");
            m_DirectionalLightController.Init(constants, 0.0f, DirectX::XMConvertToRadians(65.0f));
        }

        // Point light
        {
            Spieler::LightConstants constants;
            constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };
            constants.Position = DirectX::XMFLOAT3{ 60.0f, -7.0f, 30.0f };
            constants.FalloffStart = 35.0f;
            constants.FalloffEnd = 40.0f;

            m_PointLightController.SetConstants(&lightsConstants[1]);
            m_PointLightController.SetShape(&m_ColorRenderItems["geosphere2"]);
            m_PointLightController.SetName("Point Light Controller");
            m_PointLightController.Init(constants);
        }

        // Spot light
        {
            Spieler::LightConstants constants;
            constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };
            constants.Position = DirectX::XMFLOAT3{ -80.0f, 0.0f, 0.0f };
            constants.FalloffStart = 80.0f;
            constants.FalloffEnd = 90.0f;
            constants.SpotPower = 16.0f;

            m_SpotLightController.SetConstants(&lightsConstants[2]);
            m_SpotLightController.SetShape(&m_ColorRenderItems["cylinder"]);
            m_SpotLightController.SetName("Spot Light Controller");
            m_SpotLightController.Init(constants, -DirectX::XM_PIDIV4, DirectX::XMConvertToRadians(75.0f));
        }
    }

    void LandLayer::InitViewport()
    {
        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(m_Window.GetWidth());
        m_Viewport.Height = static_cast<float>(m_Window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void LandLayer::InitScissorRect()
    {
        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(m_Window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(m_Window.GetHeight());
    }

    bool LandLayer::OnWindowResized(Spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();

        return false;
    }

    void LandLayer::UpdateWavesVertexBuffer(float dt)
    {
        static Spieler::MaterialConstants& waterMaterialConstants{ m_Materials["water"].ConstantBuffer.As<Spieler::MaterialConstants>() };

        waterMaterialConstants.Transform = DirectX::XMMatrixTranspose(waterMaterialConstants.Transform);
        float tu = DirectX::XMVectorGetX(waterMaterialConstants.Transform.r[3]);
        float tv = DirectX::XMVectorGetY(waterMaterialConstants.Transform.r[3]);

        tu += 0.005f * dt;
        tv += 0.009f * dt;

        if (tu >= 1.0f)
        {
            tu -= 1.0f;
        }

        if (tv >= 1.0f)
        {
            tv -= 1.0f;
        }

        waterMaterialConstants.Transform.r[3].m128_f32[0] = tu;
        waterMaterialConstants.Transform.r[3].m128_f32[1] = tv;
        waterMaterialConstants.Transform = DirectX::XMMatrixTranspose(waterMaterialConstants.Transform);

        static Vertex* vertices{ &m_UploadBuffers["waves"].As<Vertex>(0) };

        for (std::uint32_t i = 0; i < m_Waves.GetVertexCount(); ++i)
        {
            // TODO: Vertex& vertex{ vertices[i] } work in several times slower!
            Vertex vertex;
            vertex.Position = m_Waves.GetPosition(i);
            vertex.Normal = m_Waves.GetNormal(i);

#if 1
            const float x{ vertex.Position.x };
            const float z{ vertex.Position.z };
#else
            const float x{ 46.20f };
            const float z{ 1.0f };
#endif

            vertex.TexCoord = DirectX::XMFLOAT2{ 0.5f + x / m_Waves.GetWidth(), 0.5f - z / m_Waves.GetDepth() };

            vertices[i] = vertex;
        }
    }

    bool LandLayer::RenderLitRenderItems()
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

            m_Renderer.ClearRenderTarget(m_ClearColor);
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
            m_DescriptorHeaps["srv"].Bind();
            m_PassConstantBuffer.Bind(0);

            for (const auto& [renderItemName, renderItem] : m_LitRenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                m_VertexBufferViews[renderItemName].Bind();
                m_IndexBufferViews[renderItemName].Bind();

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

        return true;
    }

    bool LandLayer::RenderColorRenderItems()
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineStates["color"]));
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

            const auto& landGeometry = m_MeshGeometries["land"];

            m_RootSignatures["default"].Bind();
            m_PassConstantBuffer.Bind(0);

            for (const auto& [renderItemName, renderItem] : m_ColorRenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                m_VertexBufferViews[renderItemName].Bind();
                m_IndexBufferViews[renderItemName].Bind();

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

} // namespace Sandbox
