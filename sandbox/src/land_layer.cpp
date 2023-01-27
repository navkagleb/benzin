#include "bootstrap.hpp"

#include "land_layer.hpp"

#if 0

#include <random>
#include <chrono>

#include <DirectXColors.h>

#include <imgui/imgui.h>

#include <benzin/application.hpp>
#include <benzin/common.hpp>
#include <benzin/math.hpp>
#include <benzin/window/input.hpp>
#include <benzin/window/event_dispatcher.hpp>
#include <benzin/window/window_event.hpp>
#include <benzin/window/mouse_event.hpp>
#include <benzin/window/key_event.hpp>
#include <benzin/utility/random.hpp>
#include <benzin/graphics/geometry_generator.hpp>
#include <benzin/graphics/sampler.hpp>
#include <benzin/graphics/rasterizer_state.hpp>
#include <benzin/graphics/blend_state.hpp>

namespace sandbox
{

    constexpr std::uint32_t MAX_LIGHT_COUNT{ 16 };

    struct PassConstants
    {
        alignas(16) DirectX::XMMATRIX View{};
        alignas(16) DirectX::XMMATRIX Projection{};
        alignas(16) DirectX::XMFLOAT3 CameraPosition{};
        alignas(16) DirectX::XMFLOAT4 AmbientLight{};
        alignas(16) benzin::LightConstants Lights[MAX_LIGHT_COUNT];
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

    LandLayer::LandLayer(benzin::Window& window, benzin::Renderer& renderer)
        : m_Window(window)
        , m_Renderer(renderer)
        , m_CameraController(DirectX::XM_PIDIV2, m_Window.GetAspectRatio())
        , m_Waves(WavesProps{ 400, 400, 0.03f, 0.4f, 4.0f, 0.2f })
    {}

    bool LandLayer::OnAttach()
    {
        benzin::Reference<benzin::UploadBuffer> uploadBuffer{ benzin::UploadBuffer::Create(benzin::UploadBufferType::Default, benzin::MB(10)) };

        SPIELER_RETURN_IF_FAILED(InitDescriptorHeaps());
        SPIELER_RETURN_IF_FAILED(InitUploadBuffers());

        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures(*uploadBuffer));

            InitMaterials();

            SPIELER_RETURN_IF_FAILED(InitMeshGeometry(*uploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitLandGeometry(*uploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitWavesGeometry(*uploadBuffer));
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

    void LandLayer::OnEvent(benzin::Event& event)
    {
        benzin::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<benzin::WindowResizedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnWindowResized));

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
        
        if ((benzin::Application::GetInstance().GetTimer().GetTotalTime() - time) >= 0.25f)
        {
            time += 0.25f;

            const auto i = benzin::Random::GetInstance().GetIntegral<std::uint32_t>(4, m_Waves.GetRowCount() - 5);
            const auto j = benzin::Random::GetInstance().GetIntegral<std::uint32_t>(4, m_Waves.GetColumnCount() - 5);
            const auto r = benzin::Random::GetInstance().GetFloatingPoint<float>(0.2f, 0.5f);

            m_Waves.Disturb(i, j, r);
        }

        m_Waves.OnUpdate(dt);

        UpdateWavesVertexBuffer(dt);
    }

    bool LandLayer::OnRender(float dt)
    {
        SPIELER_RETURN_IF_FAILED(RenderLitRenderItems());
        SPIELER_RETURN_IF_FAILED(RenderBlendedRenderItems());
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
        SPIELER_RETURN_IF_FAILED(m_DescriptorHeaps["srv"].Init(benzin::DescriptorHeapType::SRV, 2));

        return true;
    }

    bool LandLayer::InitUploadBuffers()
    {
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["pass"] = benzin::UploadBuffer::Create<PassConstants>(benzin::UploadBufferType::ConstantBuffer, 1));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["object"] = benzin::UploadBuffer::Create<ObjectConstants>(benzin::UploadBufferType::ConstantBuffer, m_RenderItemCount));
        SPIELER_RETURN_IF_FAILED(m_UploadBuffers["material"] = benzin::UploadBuffer::Create<benzin::MaterialConstants>(benzin::UploadBufferType::ConstantBuffer, m_MaterialCount));

        return true;
    }

    bool LandLayer::InitTextures(benzin::UploadBuffer& textureUploadBuffer)
    {
        SPIELER_RETURN_IF_FAILED(m_Textures["grass"].LoadFromDDSFile(L"assets/textures/grass.dds", textureUploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["water"].LoadFromDDSFile(L"assets/textures/water.dds", textureUploadBuffer));

        return true;
    }

    void LandLayer::InitMaterials()
    {
        const benzin::Reference<benzin::UploadBuffer> materialUploadBuffer{ m_UploadBuffers["material"] };
        const auto& descriptorHeap{ m_DescriptorHeaps["srv"] };

        // Grass material
        {
            benzin::Material& grass{ m_Materials["grass"] };
            grass.DiffuseMap.Init(m_Textures["grass"], descriptorHeap.GetDescriptorHandle(0));
            grass.ConstantBuffer.Init(*materialUploadBuffer, 0);

            benzin::MaterialConstants& grassConstants{ grass.ConstantBuffer.As<benzin::MaterialConstants>() };
            grassConstants.DiffuseAlbedo = DirectX::XMFLOAT4{ 0.2f, 0.6f, 0.2f, 1.0f };
            grassConstants.FresnelR0 = DirectX::XMFLOAT3{ 0.01f, 0.01f, 0.01f };
            grassConstants.Roughness = 0.9f;
            grassConstants.Transform = DirectX::XMMatrixIdentity();
        }

        // Water material
        {
            benzin::Material& water{ m_Materials["water"] };
            water.DiffuseMap.Init(m_Textures["water"], descriptorHeap.GetDescriptorHandle(1));
            water.ConstantBuffer.Init(*materialUploadBuffer, 1);

            benzin::MaterialConstants& waterConstants{ water.ConstantBuffer.As<benzin::MaterialConstants>() };
            waterConstants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f };
            waterConstants.FresnelR0 = DirectX::XMFLOAT3{ 0.2f, 0.2f, 0.2f };
            waterConstants.Roughness = 0.0f;
            waterConstants.Transform = DirectX::XMMatrixIdentity();
        }
    }

    bool LandLayer::InitMeshGeometry(benzin::UploadBuffer& uploadBuffer)
    {
        benzin::BoxGeometryProps boxProps;
        boxProps.Width = 30.0f;
        boxProps.Height = 20.0f;
        boxProps.Depth = 25.0f;
        boxProps.SubdivisionCount = 2;

        benzin::GridGeometryProps gridProps;
        gridProps.Width = 100.0f;
        gridProps.Depth = 100.0f;
        gridProps.RowCount = 10;
        gridProps.ColumnCount = 10;

        benzin::CylinderGeometryProps cylinderProps;
        cylinderProps.TopRadius = 10.0f;
        cylinderProps.BottomRadius = 20.0f;
        cylinderProps.Height = 40.0f;
        cylinderProps.SliceCount = 20;
        cylinderProps.StackCount = 3;

        benzin::SphereGeometryProps sphereProps;
        sphereProps.Radius = 20.0f;
        sphereProps.SliceCount = 30;
        sphereProps.StackCount = 10;

        benzin::GeosphereGeometryProps geosphereProps;
        geosphereProps.Radius = 20.0f;
        geosphereProps.SubdivisionCount = 2;

        const benzin::MeshData boxMeshData = benzin::GeometryGenerator::GenerateBox<benzin::BasicVertex, std::uint32_t>(boxProps);
        const benzin::MeshData gridMeshData = benzin::GeometryGenerator::GenerateGrid<benzin::BasicVertex, std::uint32_t>(gridProps);
        const benzin::MeshData cylinderMeshData = benzin::GeometryGenerator::GenerateCylinder<benzin::BasicVertex, std::uint32_t>(cylinderProps);
        const benzin::MeshData sphereMeshData = benzin::GeometryGenerator::GenerateSphere<benzin::BasicVertex, std::uint32_t>(sphereProps);
        const benzin::MeshData geosphereMeshData = benzin::GeometryGenerator::GenerateGeosphere<benzin::BasicVertex, std::uint32_t>(geosphereProps);

        auto& meshes{ m_MeshGeometries["meshes"] };

        benzin::SubmeshGeometry& boxSubmesh{ meshes.Submeshes["box"] };
        boxSubmesh.IndexCount = static_cast<std::uint32_t>(boxMeshData.Indices.size());
        boxSubmesh.StartIndexLocation = 0;
        boxSubmesh.BaseVertexLocation = 0;

        benzin::SubmeshGeometry& gridSubmesh{ meshes.Submeshes["grid"] };
        gridSubmesh.IndexCount = static_cast<std::uint32_t>(gridMeshData.Indices.size());
        gridSubmesh.BaseVertexLocation = boxSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(boxMeshData.Vertices.size());
        gridSubmesh.StartIndexLocation = boxSubmesh.StartIndexLocation + static_cast<std::uint32_t>(boxMeshData.Indices.size());

        benzin::SubmeshGeometry& cylinderSubmesh{ meshes.Submeshes["cylinder"] };
        cylinderSubmesh.IndexCount = static_cast<std::uint32_t>(cylinderMeshData.Indices.size());
        cylinderSubmesh.BaseVertexLocation = gridSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(gridMeshData.Vertices.size());
        cylinderSubmesh.StartIndexLocation = gridSubmesh.StartIndexLocation + static_cast<std::uint32_t>(gridMeshData.Indices.size());

        benzin::SubmeshGeometry& sphereSubmesh{ meshes.Submeshes["sphere"] };
        sphereSubmesh.IndexCount = static_cast<std::uint32_t>(sphereMeshData.Indices.size());
        sphereSubmesh.BaseVertexLocation = cylinderSubmesh.BaseVertexLocation + static_cast<std::uint32_t>(cylinderMeshData.Vertices.size());
        sphereSubmesh.StartIndexLocation = cylinderSubmesh.StartIndexLocation + static_cast<std::uint32_t>(cylinderMeshData.Indices.size());

        benzin::SubmeshGeometry& geosphereSubmesh{ meshes.Submeshes["geosphere"] };
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

        SPIELER_RETURN_IF_FAILED(meshes.InitStaticVertexBuffer(vertices.data(), vertices.size(), uploadBuffer));
        SPIELER_RETURN_IF_FAILED(meshes.InitStaticIndexBuffer(indices.data(), indices.size(), uploadBuffer));

        meshes.m_PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

#if 0
        // Render items
        benzin::RenderItem box;
        box.MeshGeometry        = &meshes;
        box.SubmeshGeometry     = &meshes.Submeshes["box"];
        
        SPIELER_RETURN_IF_FAILED(box.ConstantBuffer.InitAsRootDescriptor(&m_ObjectUploadBuffer, 0));
        m_ColorRenderItems.push_back(std::move(box));
#endif

#if 0
        benzin::RenderItem sphere;
        sphere.MeshGeometry           = &meshes;
        sphere.SubmeshGeometry        = &meshes.Submeshes["sphere"];

        SPIELER_RETURN_IF_FAILED(sphere.ConstantBuffer.InitAsRootDescriptor(&m_ObjectUploadBuffer, 2));
        m_ColorRenderItems.push_back(std::move(sphere));
#endif

        // For m_DirectionalLight
        {
            const std::string renderItemName{ "geosphere1" };

            benzin::RenderItem& geosphere{ m_ColorRenderItems[renderItemName] };
            geosphere.MeshGeometry = &meshes;
            geosphere.SubmeshGeometry = &meshes.Submeshes["geosphere"];
            geosphere.ConstantBuffer.Init(*m_UploadBuffers["object"], 2);

            ObjectConstants& geosphereConstants{ geosphere.ConstantBuffer.As<ObjectConstants>() };
            geosphereConstants.World = DirectX::XMMatrixIdentity();
            geosphereConstants.TextureTransform = DirectX::XMMatrixIdentity();
        }

        // For m_PointLight
        {
            const std::string renderItemName{ "geosphere2" };

            benzin::RenderItem& geosphere{ m_ColorRenderItems[renderItemName]};
            geosphere.MeshGeometry = &meshes;
            geosphere.SubmeshGeometry = &meshes.Submeshes["geosphere"];
            geosphere.ConstantBuffer.Init(*m_UploadBuffers["object"], 3);

            ObjectConstants& geosphereConstants{ geosphere.ConstantBuffer.As<ObjectConstants>() };
            geosphereConstants.World = DirectX::XMMatrixIdentity();
            geosphereConstants.TextureTransform = DirectX::XMMatrixIdentity();
        }

        // For m_SpotLight
        {
            const std::string renderItemName{ "cylinder" };

            benzin::RenderItem& cylinder{ m_ColorRenderItems["cylinder"] };
            cylinder.MeshGeometry = &meshes;
            cylinder.SubmeshGeometry = &meshes.Submeshes["cylinder"];
            cylinder.ConstantBuffer.Init(*m_UploadBuffers["object"], 4);

            ObjectConstants& cylinderConstants{ cylinder.ConstantBuffer.As<ObjectConstants>() };
            cylinderConstants.World = DirectX::XMMatrixIdentity();
            cylinderConstants.TextureTransform = DirectX::XMMatrixIdentity();
        }

        return true;
    }

    bool LandLayer::InitLandGeometry(benzin::UploadBuffer& uploadBuffer)
    {
        benzin::GridGeometryProps gridGeometryProps;
        gridGeometryProps.Width = 160.0f;
        gridGeometryProps.Depth = 160.0f;
        gridGeometryProps.RowCount = 100;
        gridGeometryProps.ColumnCount = 100;

        const auto& gridMeshData = benzin::GeometryGenerator::GenerateGrid<benzin::BasicVertex, std::uint32_t>(gridGeometryProps);

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

        SPIELER_RETURN_IF_FAILED(landMeshGeometry.InitStaticVertexBuffer(vertices.data(), vertices.size(), uploadBuffer));
        SPIELER_RETURN_IF_FAILED(landMeshGeometry.InitStaticIndexBuffer(gridMeshData.Indices.data(), gridMeshData.Indices.size(), uploadBuffer));

        landMeshGeometry.m_PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

        auto& landMesh = landMeshGeometry.Submeshes["land"];
        landMesh.IndexCount = gridMeshData.Indices.size();

        // Render items
        benzin::RenderItem& land{ m_LitRenderItems["land"] };
        land.MeshGeometry = &landMeshGeometry;
        land.SubmeshGeometry = &landMeshGeometry.Submeshes["land"];
        land.Material = &m_Materials["grass"];
        land.ConstantBuffer.Init(*m_UploadBuffers["object"], 0);

        ObjectConstants& landConstants{ land.ConstantBuffer.As<ObjectConstants>() };
        landConstants.World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -20.0f, 0.0f));
        landConstants.TextureTransform = DirectX::XMMatrixScaling(5.0f, 5.0f, 1.0f);

        return true;
    }

    bool LandLayer::InitWavesGeometry(benzin::UploadBuffer& uploadBuffer)
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

        SPIELER_RETURN_IF_FAILED(wavesMeshGeometry.InitDynamicVertexBuffer<Vertex>(m_Waves.GetVertexCount()));
        SPIELER_RETURN_IF_FAILED(wavesMeshGeometry.InitStaticIndexBuffer(indices.data(), static_cast<std::uint32_t>(indices.size()), uploadBuffer));

        wavesMeshGeometry.m_PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

        auto& submesh{ wavesMeshGeometry.Submeshes["main"] };
        submesh.IndexCount = static_cast<std::uint32_t>(indices.size());
        submesh.StartIndexLocation = 0;
        submesh.BaseVertexLocation = 0;

        // Render Item
        benzin::RenderItem& waves{ m_BlendedRenderItems["waves"] };
        waves.MeshGeometry = &wavesMeshGeometry;
        waves.SubmeshGeometry = &wavesMeshGeometry.Submeshes["main"];
        waves.Material = &m_Materials["water"];

        waves.ConstantBuffer.Init(*m_UploadBuffers["object"], 1);
        waves.ConstantBuffer.As<ObjectConstants>().World = DirectX::XMMatrixTranspose(DirectX::XMMatrixTranslation(0.0f, -25.0f, 0.0f));
        waves.ConstantBuffer.As<ObjectConstants>().TextureTransform = DirectX::XMMatrixScaling(10.0f, 10.0f, 1.0f);

        return true;
    }

    bool LandLayer::InitRootSignature()
    {
        std::vector<benzin::RootParameter> rootParameters;
        std::vector<benzin::StaticSampler> staticSamplers;

        // Init root parameters
        {
            rootParameters.resize(4);

            rootParameters[0].Type = benzin::RootParameterType_ConstantBufferView;
            rootParameters[0].ShaderVisibility = benzin::ShaderVisibility_All;
            rootParameters[0].Child = benzin::RootDescriptor{ 0, 0 };

            rootParameters[1].Type = benzin::RootParameterType_ConstantBufferView;
            rootParameters[1].ShaderVisibility = benzin::ShaderVisibility_All;
            rootParameters[1].Child = benzin::RootDescriptor{ 1, 0 };

            rootParameters[2].Type = benzin::RootParameterType_ConstantBufferView;
            rootParameters[2].ShaderVisibility = benzin::ShaderVisibility_All;
            rootParameters[2].Child = benzin::RootDescriptor{ 2, 0 };

            rootParameters[3].Type = benzin::RootParameterType_DescriptorTable;
            rootParameters[3].ShaderVisibility = benzin::ShaderVisibility_All;
            rootParameters[3].Child = benzin::RootDescriptorTable{ { benzin::DescriptorRange{ benzin::DescriptorRangeType_SRV, 1 } } };
        }

        // Init statis samplers
        {
            staticSamplers.resize(6);

            staticSamplers[0] = benzin::StaticSampler(benzin::TextureFilterType_Point, benzin::TextureAddressMode_Wrap, 0);
            staticSamplers[1] = benzin::StaticSampler(benzin::TextureFilterType_Point, benzin::TextureAddressMode_Clamp, 1);
            staticSamplers[2] = benzin::StaticSampler(benzin::TextureFilterType_Linear, benzin::TextureAddressMode_Wrap, 2);
            staticSamplers[3] = benzin::StaticSampler(benzin::TextureFilterType_Linear, benzin::TextureAddressMode_Clamp, 3);
            staticSamplers[4] = benzin::StaticSampler(benzin::TextureFilterType_Anisotropic, benzin::TextureAddressMode_Wrap, 4);
            staticSamplers[5] = benzin::StaticSampler(benzin::TextureFilterType_Anisotropic, benzin::TextureAddressMode_Clamp, 5);
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

            const std::vector<benzin::ShaderDefine> defines
            {
                benzin::ShaderDefine{ "DIRECTIONAL_LIGHT_COUNT", "1" },
                benzin::ShaderDefine{ "POINT_LIGHT_COUNT", "1" },
                benzin::ShaderDefine{ "SPOT_LIGHT_COUNT", "1" },
            };

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/default.fx", "VS_Main", defines));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/default.fx", "PS_Main", defines));

            benzin::RasterizerState rasterizerState;
            rasterizerState.FillMode = benzin::FillMode::Solid;
            rasterizerState.CullMode = benzin::CullMode::Back;

            benzin::InputLayout inputLayout
            {
                benzin::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                benzin::InputLayoutElement{ "Normal", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                benzin::InputLayoutElement{ "TexCoord", DXGI_FORMAT_R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
            };

            benzin::PipelineStateProps pipelineStateProps;
            pipelineStateProps.RootSignature = &m_RootSignatures["default"];
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.RasterizerState = &rasterizerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["lit"].Init(pipelineStateProps));
        }

        // Blended PSO
        {
            auto& vertexShader = m_VertexShaders["default"];
            auto& pixelShader = m_PixelShaders["default"];

            benzin::RasterizerState rasterizerState;
            rasterizerState.FillMode = benzin::FillMode::Solid;
            rasterizerState.CullMode = benzin::CullMode::Back;

            benzin::RenderTargetBlendProps renderTargetBlendProps;
            renderTargetBlendProps.Type = benzin::RenderTargetBlendingType::Default;
            renderTargetBlendProps.Channels = benzin::BlendChannel_All;
            renderTargetBlendProps.ColorEquation.SourceFactor = benzin::BlendColorFactor::SourceAlpha;
            renderTargetBlendProps.ColorEquation.DestinationFactor = benzin::BlendColorFactor::InverseSourceAlpha;
            renderTargetBlendProps.ColorEquation.Operation = benzin::BlendOperation::Add;
            renderTargetBlendProps.AlphaEquation.SourceFactor = benzin::BlendAlphaFactor::One;
            renderTargetBlendProps.AlphaEquation.DestinationFactor = benzin::BlendAlphaFactor::Zero;
            renderTargetBlendProps.AlphaEquation.Operation = benzin::BlendOperation::Add;

            benzin::BlendState blendState;
            blendState.RenderTargetProps.push_back(renderTargetBlendProps);

            benzin::InputLayout inputLayout
            {
                benzin::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                benzin::InputLayoutElement{ "Normal", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                benzin::InputLayoutElement{ "TexCoord", DXGI_FORMAT_R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
            };

            benzin::PipelineStateProps pipelineStateProps;
            pipelineStateProps.RootSignature = &m_RootSignatures["default"];
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.BlendState = &blendState;
            pipelineStateProps.RasterizerState = &rasterizerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["blend"].Init(pipelineStateProps));
        }

        // Color PSO
        {
            auto& vertexShader = m_VertexShaders["color"];
            auto& pixelShader = m_PixelShaders["color"];

            SPIELER_RETURN_IF_FAILED(vertexShader.LoadFromFile(L"assets/shaders/color1.fx", "VS_Main"));
            SPIELER_RETURN_IF_FAILED(pixelShader.LoadFromFile(L"assets/shaders/color1.fx", "PS_Main"));

            benzin::RasterizerState rasterzerState;
            rasterzerState.FillMode = benzin::FillMode::Wireframe;
            rasterzerState.CullMode = benzin::CullMode::None;

            benzin::InputLayout inputLayout
            {
                benzin::InputLayoutElement{ "Position", DXGI_FORMAT_R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                benzin::InputLayoutElement{ "Color", DXGI_FORMAT_R32G32B32A32_FLOAT, sizeof(DirectX::XMFLOAT4) }
            };

            benzin::PipelineStateProps pipelineStateProps;
            pipelineStateProps.RootSignature = &m_RootSignatures["default"];
            pipelineStateProps.VertexShader = &vertexShader;
            pipelineStateProps.PixelShader = &pixelShader;
            pipelineStateProps.RasterizerState = &rasterzerState;
            pipelineStateProps.InputLayout = &inputLayout;
            pipelineStateProps.PrimitiveTopologyType = benzin::PrimitiveTopologyType::Triangle;
            pipelineStateProps.RTVFormat = m_Renderer.GetSwapChainProps().BufferFormat;
            pipelineStateProps.DSVFormat = m_Renderer.GetDepthStencilFormat();

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["color"].Init(pipelineStateProps));
        }

        return true;
    }

    void LandLayer::InitPassConstantBuffer()
    {
        m_PassConstantBuffer.Init(*m_UploadBuffers["pass"], 0);

        auto& passConstants{ m_PassConstantBuffer.As<PassConstants>() };
        passConstants.AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
        passConstants.FogColor = m_ClearColor;
        passConstants.FogStart = 50.0f;
        passConstants.FogRange = 110.0f;
    }

    void LandLayer::InitLightControllers()
    {
        auto& lightsConstants = m_PassConstantBuffer.As<PassConstants>().Lights;

        // Directional light
        {
            benzin::LightConstants constants;
            constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.7f };

            m_DirectionalLightController.SetConstants(&lightsConstants[0]);
            m_DirectionalLightController.SetShape(&m_ColorRenderItems["geosphere1"]);
            m_DirectionalLightController.SetName("Sun Controller");
            m_DirectionalLightController.Init(constants, 0.0f, DirectX::XMConvertToRadians(65.0f));
        }

        // Point light
        {
            benzin::LightConstants constants;
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
            benzin::LightConstants constants;
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

    bool LandLayer::OnWindowResized(benzin::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();

        return false;
    }

    void LandLayer::UpdateWavesVertexBuffer(float dt)
    {
        static benzin::MaterialConstants& waterMaterialConstants{ m_Materials["water"].ConstantBuffer.As<benzin::MaterialConstants>() };

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

        static Vertex* vertices{ &m_MeshGeometries["waves"].GetDynamicVertexBuffer()->As<Vertex>(0)};

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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorHandle(currentBackBuffer).CPU };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorHandle(0).CPU };

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

                renderItem.MeshGeometry->GetVertexBufferView().Bind();
                renderItem.MeshGeometry->GetIndexBufferView().Bind();

                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->m_PrimitiveTopology));

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

    bool LandLayer::RenderBlendedRenderItems()
    {
        SPIELER_RETURN_IF_FAILED(m_Renderer.m_CommandAllocator->Reset());
        SPIELER_RETURN_IF_FAILED(m_Renderer.ResetCommandList(m_PipelineStates["blend"]));
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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorHandle(currentBackBuffer).CPU };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorHandle(0).CPU };

            m_Renderer.m_CommandList->OMSetRenderTargets(
                1,
                &a,
                true,
                &b
            );

            m_RootSignatures["default"].Bind();
            m_DescriptorHeaps["srv"].Bind();
            m_PassConstantBuffer.Bind(0);

            for (const auto& [renderItemName, renderItem] : m_BlendedRenderItems)
            {
                const auto& submeshGeometry = *renderItem.SubmeshGeometry;

                renderItem.MeshGeometry->GetVertexBufferView().Bind();
                renderItem.MeshGeometry->GetIndexBufferView().Bind();

                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->m_PrimitiveTopology));

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

            const D3D12_CPU_DESCRIPTOR_HANDLE a{ m_Renderer.m_SwapChainBufferRTVDescriptorHeap.GetDescriptorHandle(currentBackBuffer).CPU };
            const D3D12_CPU_DESCRIPTOR_HANDLE b{ m_Renderer.m_DSVDescriptorHeap.GetDescriptorHandle(0).CPU };

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

                renderItem.MeshGeometry->GetVertexBufferView().Bind();
                renderItem.MeshGeometry->GetIndexBufferView().Bind();

                m_Renderer.m_CommandList->IASetPrimitiveTopology(static_cast<D3D12_PRIMITIVE_TOPOLOGY>(renderItem.MeshGeometry->m_PrimitiveTopology));

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

} // namespace sandbox

#endif
