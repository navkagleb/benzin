#include "bootstrap.hpp"

#include "test_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/core/logger.hpp>

#include <spieler/system/event.hpp>
#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/blend_state.hpp>
#include <spieler/renderer/rasterizer_state.hpp>
#include <spieler/renderer/resource_barrier.hpp>
#include <spieler/renderer/depth_stencil_state.hpp>

#include <spieler/math/matrix.hpp>

#include <spieler/utility/random.hpp>

namespace sandbox
{

    namespace cb
    {

        struct Pass
        {
            alignas(16) DirectX::XMMATRIX View{};
            alignas(16) DirectX::XMMATRIX Projection{};
            alignas(16) DirectX::XMFLOAT3 CameraPosition{};
            alignas(16) DirectX::XMFLOAT4 AmbientLight{};
            alignas(16) spieler::renderer::LightConstants Lights[MAX_LIGHT_COUNT];
            
            alignas(16) DirectX::XMFLOAT4 FogColor{};
            float ForStart{};
            float FogRange{};
        };

        struct ColorObject
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
        };

        struct Object
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            DirectX::XMMATRIX TextureTransform{ DirectX::XMMatrixIdentity() };
        };

        struct Material
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
        };

    } // namespace cb

#if 0
    namespace per
    {

        enum class Texture
        {
            DIRECTIONAL_LIGHT_COUNT,
            USE_ALPHA_TEST,
            ENABLE_FOG
        };

        enum class Billboard
        {
            DIRECTIONAL_LIGHT_COUNT,
            USE_ALPHA_TEST,
            ENABLE_FOG
        };

    } // namespace per
#endif

    struct ColorConstants
    {
        DirectX::XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
    };

    struct MirrorConstants
    {
        DirectX::XMVECTOR Plane{ 0.0f, 0.0f, 0.0f, 0.0f };
    };

    TestLayer::TestLayer(spieler::Window& window)
        : m_Window(window)
        , m_CameraController(spieler::math::ToRadians(60.0f), m_Window.GetAspectRatio())
    {}

    bool TestLayer::OnAttach()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        spieler::renderer::UploadBuffer uploadBuffer(device, spieler::renderer::UploadBufferType::Default, spieler::MB(6));

        UpdateViewport();
        UpdateScissorRect();

        InitConstantBuffers();

        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {
            SPIELER_RETURN_IF_FAILED(InitTextures(uploadBuffer));
            SPIELER_RETURN_IF_FAILED(InitMeshGeometries(uploadBuffer));
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));

        InitMaterials();
        SPIELER_RETURN_IF_FAILED(InitRootSignatures());
        SPIELER_RETURN_IF_FAILED(InitPipelineStates());

        InitRenderItems();
        InitLights();

        return true;
    }

    void TestLayer::OnEvent(spieler::Event& event)
    {
        spieler::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        m_CameraController.OnEvent(event);
    }

    void TestLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        const ProjectionCamera& camera{ m_CameraController.GetCamera() };
        
        // Update Pass Constants
        {
            const ProjectionCamera& camera{ m_CameraController.GetCamera() };

            m_PassConstants["direct"].View = camera.View;
            m_PassConstants["direct"].Projection = camera.Projection;
            DirectX::XMStoreFloat3(&m_PassConstants["direct"].CameraPosition, camera.EyePosition);

            // Reflect light
            {
                const DirectX::XMMATRIX reflectOperator{ DirectX::XMMatrixReflect(m_RenderItems["mirror"]->GetComponent<MirrorConstants>().Plane) };

                PassConstants& direct{ m_PassConstants["direct"] };
                PassConstants& reflected{ m_PassConstants["reflected"] };

                reflected = direct;

                for (uint32_t i = 0; i < MAX_LIGHT_COUNT; ++i)
                {
                    const DirectX::XMVECTOR directlightDirection{ DirectX::XMLoadFloat3(&direct.Lights[i].Direction) };
                    const DirectX::XMVECTOR reflectedLightDirection{ DirectX::XMVector3TransformNormal(directlightDirection, reflectOperator) };

                    DirectX::XMStoreFloat3(&reflected.Lights[i].Direction, reflectedLightDirection);
                }
            }

            // ConstantBuffer
            for (const auto& [name, pass] : m_PassConstants)
            {
                cb::Pass constants
                {
                    .View = DirectX::XMMatrixTranspose(pass.View),
                    .Projection = DirectX::XMMatrixTranspose(pass.Projection),
                    .CameraPosition = pass.CameraPosition,
                    .AmbientLight = pass.AmbientLight,
                    .FogColor = pass.Fog.Color,
                    .ForStart = pass.Fog.Start,
                    .FogRange = pass.Fog.Range
                };

                for (uint32_t i = 0; i < MAX_LIGHT_COUNT; ++i)
                {
                    constants.Lights[i] = pass.Lights[i];
                }

                m_ConstantBuffers["pass"].GetSlice(&pass).Update(&constants, sizeof(constants) );
            }
        }

        // Update RenderItems
        {
            m_RenderItems["skull"]->Transform.Rotation.x += 1.0f * dt;
            m_RenderItems["skull"]->Transform.Rotation.y += 1.0f * dt;

            // ConstantBuffer
            for (const auto& [name, item] : m_RenderItems)
            {
                if (item->HasComponent<ColorConstants>())
                {
                    const cb::ColorObject constants
                    {
                        .World = DirectX::XMMatrixTranspose(item->Transform.GetMatrix()),
                        .Color = item->GetComponent<ColorConstants>().Color
                    };

                    m_ConstantBuffers["color_render_item"].GetSlice(item.get()).Update(&constants, sizeof(constants));
                }
                else
                {
                    cb::Object constants
                    {
                        .TextureTransform = DirectX::XMMatrixTranspose(item->TextureTransform.GetMatrix())
                    };

                    if (name == "skull_shadow")
                    {
                        const DirectX::XMFLOAT3 lightDirection{ m_PassConstants["direct"].Lights[0].Direction };

                        const DirectX::XMVECTOR shadowPlane{ 0.0f, 1.0f, 0.0f, 0.0f };
                        const DirectX::XMVECTOR toLight{ -lightDirection.x, -lightDirection.y, -lightDirection.z, 0.0f };
                        const DirectX::XMMATRIX shadowOperator{ DirectX::XMMatrixShadow(shadowPlane, toLight) };
                        const DirectX::XMMATRIX translation{ DirectX::XMMatrixTranslation(0.0f, 0.001f, 0.0f) };

                        constants.World = DirectX::XMMatrixTranspose(m_RenderItems["skull"]->Transform.GetMatrix() * shadowOperator * translation);
                    }
                    else if (name == "reflected_skull")
                    {
                        const DirectX::XMMATRIX reflectOperator{ DirectX::XMMatrixReflect(m_RenderItems["mirror"]->GetComponent<MirrorConstants>().Plane) };

                        constants.World = DirectX::XMMatrixTranspose(m_RenderItems["skull"]->Transform.GetMatrix() * reflectOperator);
                    }
                    else
                    {
                        constants.World = DirectX::XMMatrixTranspose(item->Transform.GetMatrix());
                    }

                    if (auto& constantBuffer{ m_ConstantBuffers["render_item"] }; constantBuffer.HasSlice(item.get()))
                    {
                        constantBuffer.GetSlice(item.get()).Update(&constants, sizeof(constants));
                    }
                }
            }
        }

        // Update Materials
        {
            for (const auto& [name, material] : m_Materials)
            {
                const cb::Material constants
                {
                    .DiffuseAlbedo = material.Constants.DiffuseAlbedo,
                    .FresnelR0 = material.Constants.FresnelR0,
                    .Roughness = material.Constants.Roughness,
                    .Transform = material.Transform.GetMatrix()
                };

                m_ConstantBuffers["material"].GetSlice(&material).Update(&constants, sizeof(constants));
            }
        }
    }

    bool TestLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& context{ renderer.GetContext() };

        SPIELER_RETURN_IF_FAILED(context.ResetCommandAllocator());
        SPIELER_RETURN_IF_FAILED(context.ResetCommandList());
        {
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &renderer.GetSwapChain().GetCurrentBuffer().GetResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::RenderTarget
            });
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &renderer.GetSwapChain().GetDepthStencil().GetResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::DepthWrite
            });

            renderer.SetDefaultRenderTargets();
            renderer.ClearRenderTarget({ 0.1f, 0.1f, 0.1f, 1.0f });
            renderer.ClearDepthStencil(1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            Render(m_Lights, m_PipelineStates["light"], m_PassConstants["direct"], &m_ConstantBuffers["color_render_item"]);
            Render(m_OpaqueObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            context.SetStencilReferenceValue(1);
            Render(m_Mirrors, m_PipelineStates["mirror"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            Render(m_ReflectedObjects, m_PipelineStates["reflected"], m_PassConstants["reflected"], &m_ConstantBuffers["render_item"]);

            Render(m_AlphaTestedBillboards, m_PipelineStates["billboard"], m_PassConstants["direct"]);

            context.SetStencilReferenceValue(0);
            Render(m_TransparentObjects, m_PipelineStates["opaque"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            Render(m_Shadows, m_PipelineStates["shadow"], m_PassConstants["direct"], &m_ConstantBuffers["render_item"]);

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &renderer.GetSwapChain().GetCurrentBuffer().GetResource(),
                .From = spieler::renderer::ResourceState::RenderTarget,
                .To = spieler::renderer::ResourceState::Present
            });
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &renderer.GetSwapChain().GetDepthStencil().GetResource(),
                .From = spieler::renderer::ResourceState::DepthWrite,
                .To = spieler::renderer::ResourceState::Present
            });
        }
        SPIELER_RETURN_IF_FAILED(context.ExecuteCommandList(true));

        return true;
    }

    void TestLayer::OnImGuiRender(float dt)
    {
        m_CameraController.OnImGuiRender(dt);
        m_DirectionalLightController.OnImGuiRender();
        
        ImGui::Begin("Position Settings");
        {
            ImGui::DragFloat3("Skull Position", reinterpret_cast<float*>(&m_RenderItems["skull"]->Transform.Translation), 0.1f);

            // Mirror
            {
                std::unique_ptr<spieler::renderer::RenderItem>& mirror{ m_RenderItems["mirror"] };
                MirrorConstants& mirrorConstants{ mirror->GetComponent<MirrorConstants>() };

                if (ImGui::DragFloat3("Mirror Position", reinterpret_cast<float*>(&mirror->Transform.Translation), 0.1f))
                {
                    mirrorConstants.Plane = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, -mirror->Transform.Translation.z };
                }

                DirectX::XMFLOAT4 plane;
                DirectX::XMStoreFloat4(&plane, mirrorConstants.Plane);

                ImGui::Text("Mirror Plane { %.2f, %.2f, %.2f, %.2f }", plane.x, plane.y, plane.z, plane.w);
            }

        }
        ImGui::End();

        ImGui::Begin("Fog");
        {
            struct PassConstants::Fog& fog{ m_PassConstants["direct"].Fog };

            ImGui::ColorEdit4("Color", reinterpret_cast<float*>(&fog.Color));
            ImGui::SliderFloat("Start", &fog.Start, 0.1f, 100.0f);
            ImGui::SliderFloat("Range", &fog.Range, 0.1f, 300.1f);
        }
        ImGui::End();
    }

    void TestLayer::InitConstantBuffers()
    {
        using namespace magic_enum::bitwise_operators;

        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::BufferFlags constantBufferFlags{ spieler::renderer::BufferFlags::Dynamic | spieler::renderer::BufferFlags::ConstantBuffer };

        // Pass
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(PassConstants),
                .ElementCount = 2
            };

            spieler::renderer::ConstantBuffer& pass{ m_ConstantBuffers["pass"] };

            pass.SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));

            // Direct
            {
                PassConstants& direct{ m_PassConstants["direct"] };
                direct.AmbientLight = DirectX::XMFLOAT4{ 0.25f, 0.25f, 0.35f, 1.0f };
                direct.Fog.Color = DirectX::XMFLOAT4{ 0.1f, 0.1f, 0.1f, 1.0f };
                direct.Fog.Start = 10.0f;
                direct.Fog.Range = 50.0f;

                pass.SetSlice(&direct);
            }

            // Reflected
            {
                PassConstants& reflected{ m_PassConstants["reflected"] };
                reflected = m_PassConstants["direct"];

                pass.SetSlice(&reflected);
            }
        }

        // Color Objects
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::ColorObject),
                .ElementCount = 1
            };

            m_ConstantBuffers["color_render_item"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }

        // Objects
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::Object),
                .ElementCount = 10
            };

            m_ConstantBuffers["render_item"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }

        // Materials
        {
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize = sizeof(cb::Material),
                .ElementCount = 10
            };

            m_ConstantBuffers["material"].SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
        }
    }

    bool TestLayer::InitTextures(spieler::renderer::UploadBuffer& uploadBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        SPIELER_RETURN_IF_FAILED(m_Textures["white"].LoadFromDDSFile(device, context, L"assets/textures/white.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["wood_crate1"].LoadFromDDSFile(device, context, L"assets/textures/wood_crate1.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["wire_fence"].LoadFromDDSFile(device, context, L"assets/textures/wire_fence.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["tile"].LoadFromDDSFile(device, context, L"assets/textures/tile.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["bricks"].LoadFromDDSFile(device, context, L"assets/textures/bricks3.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["mirror"].LoadFromDDSFile(device, context, L"assets/textures/ice.dds", uploadBuffer));
        SPIELER_RETURN_IF_FAILED(m_Textures["tree_array"].LoadFromDDSFile(device, context, L"assets/textures/tree_array.dds", uploadBuffer));

        return true;
    }

    void TestLayer::InitMaterials()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        spieler::renderer::ConstantBuffer& materialConstantBuffer{ m_ConstantBuffers["material"] };

        // Wood crate material
        {
            spieler::renderer::Material& wood{ m_Materials["wood_crate1"] };
            wood.DiffuseMap.Init(device, m_Textures["wood_crate1"]);

            wood.Constants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            wood.Constants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
            wood.Constants.Roughness = 0.2f;

            materialConstantBuffer.SetSlice(&wood);
        }
        
        // Wire fence material
        {
            spieler::renderer::Material& fence{ m_Materials["wire_fence"] };
            fence.DiffuseMap.Init(device, m_Textures["wire_fence"]);

            fence.Constants.DiffuseAlbedo = DirectX::XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
            fence.Constants.FresnelR0 = DirectX::XMFLOAT3(0.05f, 0.05f, 0.05f);
            fence.Constants.Roughness = 0.2f;

            materialConstantBuffer.SetSlice(&fence);
        }

        // Tile material
        {
            spieler::renderer::Material& tile{ m_Materials["tile"] };
            tile.DiffuseMap.Init(device, m_Textures["tile"]);

            tile.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tile.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            tile.Constants.Roughness = 0.8f;

            materialConstantBuffer.SetSlice(&tile);
        }

        // Bricks material
        {
            spieler::renderer::Material& bricks{ m_Materials["bricks"] };
            bricks.DiffuseMap.Init(device, m_Textures["bricks"]);

            bricks.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            bricks.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            bricks.Constants.Roughness = 0.8f;

            materialConstantBuffer.SetSlice(&bricks);
        }

        // Skull material
        {
            spieler::renderer::Material& skull{ m_Materials["skull"] };
            skull.DiffuseMap.Init(device, m_Textures["white"]);

            skull.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            skull.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
            skull.Constants.Roughness = 0.3f;

            materialConstantBuffer.SetSlice(&skull);
        }

        // Mirror material
        {
            spieler::renderer::Material& mirror{ m_Materials["mirror"] };
            mirror.DiffuseMap.Init(device, m_Textures["mirror"]);

            mirror.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 0.3f };
            mirror.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.1f, 0.1f, 0.1f };
            mirror.Constants.Roughness = 0.5f;

            materialConstantBuffer.SetSlice(&mirror);
        }

        // Shadow material
        {
            spieler::renderer::Material& shadow{ m_Materials["shadow"] };
            shadow.DiffuseMap.Init(device, m_Textures["white"]);

            shadow.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 0.0f, 0.0f, 0.0f, 0.5f };
            shadow.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.001f, 0.001f, 0.001f };
            shadow.Constants.Roughness = 0.0f;

            materialConstantBuffer.SetSlice(&shadow);
        }

        // Tree material
        {
            spieler::renderer::Material& tree{ m_Materials["tree"] };
            tree.DiffuseMap.Init(device, m_Textures["tree_array"]);

            tree.Constants.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
            tree.Constants.FresnelR0 = DirectX::XMFLOAT3{ 0.01f, 0.01f, 0.01f };
            tree.Constants.Roughness = 0.125f;

            materialConstantBuffer.SetSlice(&tree);
        }
    }

    bool TestLayer::InitMeshGeometries(spieler::renderer::UploadBuffer& uploadBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        // Basic geometry
        {
            const spieler::renderer::BoxGeometryProps boxGeometryProps
            {
                .Width = 10.0f,
                .Height = 10.0f,
                .Depth = 10.0f
            };

            const spieler::renderer::GeosphereGeometryProps geosphereGeometryProps
            {
                .Radius = 10.0f,
                .SubdivisionCount = 2
            };

            const spieler::renderer::GridGeometryProps gridGeometryProps
            {
                .Width = 1.0f,
                .Depth = 1.0f,
                .RowCount = 2,
                .ColumnCount = 2
            };

            const auto& boxMeshData{ spieler::renderer::GeometryGenerator::GenerateBox<uint32_t>(boxGeometryProps) };
            const auto& geosphereMeshData{ spieler::renderer::GeometryGenerator::GenerateGeosphere<uint32_t>(geosphereGeometryProps) };
            const auto& gridMeshData{ spieler::renderer::GeometryGenerator::GenerateGrid<uint32_t>(gridGeometryProps) };

            spieler::renderer::MeshGeometry& basicMeshesGeometry{ m_MeshGeometries["basic"] };
            basicMeshesGeometry.m_PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;
            basicMeshesGeometry.GenerateFrom(
                {
                    { "box", boxMeshData },
                    { "geosphere", geosphereMeshData },
                    { "grid", gridMeshData },
                },
                uploadBuffer
                );
        }

        // Load skull model
        {
            std::ifstream fin("assets/models/skull.txt");

            SPIELER_ASSERT(fin);

            uint32_t vcount = 0;
            uint32_t tcount = 0;
            std::string ignore;

            fin >> ignore >> vcount;
            fin >> ignore >> tcount;
            fin >> ignore >> ignore >> ignore >> ignore;

            std::vector<spieler::renderer::Vertex> vertices(vcount);
            for (UINT i = 0; i < vcount; ++i)
            {
                fin >> vertices[i].Position.x >> vertices[i].Position.y >> vertices[i].Position.z;
                fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

                // Model does not have texture coordinates, so just zero them out.
                vertices[i].TexCoord = { 0.0f, 0.0f };
            }

            fin >> ignore;
            fin >> ignore;
            fin >> ignore;

            std::vector<uint32_t> indices(3 * tcount);
            for (UINT i = 0; i < tcount; ++i)
            {
                fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
            }

            fin.close();

            //
            // Pack the indices of all the meshes into one index buffer.
            //
            spieler::renderer::MeshGeometry& skullMeshGeometry{ m_MeshGeometries["skull"] };

            const spieler::renderer::BufferConfig vertexBufferConfig
            {
                .ElementSize = sizeof(spieler::renderer::Vertex),
                .ElementCount = static_cast<uint32_t>(vertices.size())
            };

            skullMeshGeometry.m_VertexBuffer.SetResource(device.CreateBuffer(vertexBufferConfig, spieler::renderer::BufferFlags::None));
            context.CopyBuffer(vertices.data(), sizeof(spieler::renderer::Vertex) * vertices.size(), uploadBuffer, *skullMeshGeometry.m_VertexBuffer.GetResource());

            const spieler::renderer::BufferConfig indexBufferConfig
            {
                .ElementSize = sizeof(uint32_t),
                .ElementCount = static_cast<uint32_t>(indices.size())
            };

            skullMeshGeometry.m_IndexBuffer.SetResource(device.CreateBuffer(indexBufferConfig, spieler::renderer::BufferFlags::None));
            context.CopyBuffer(indices.data(), sizeof(uint32_t) * indices.size(), uploadBuffer, *skullMeshGeometry.m_IndexBuffer.GetResource());

            skullMeshGeometry.m_PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

            spieler::renderer::SubmeshGeometry& submesh{ skullMeshGeometry.m_Submeshes["grid"] };
            submesh.IndexCount = static_cast<uint32_t>(indices.size());
            submesh.BaseVertexLocation = 0;
            submesh.StartIndexLocation = 0;
        }

        // Trees (billboards)
        {
            static constexpr uint32_t treeCount{ 10 };

            struct TreeVertex
            {
                DirectX::XMFLOAT3 Position{ 0.0f, 0.0f, 0.0f };
                DirectX::XMFLOAT2 Size{ 0.0f, 0.0f };
            };

            // Vertices
            std::array<TreeVertex, treeCount> vertices;

            for (TreeVertex& vertex : vertices)
            {
                const float x{ spieler::Random::GetInstance().GetFloatingPoint(-10.0f, 10.0f) };
                const float z{ spieler::Random::GetInstance().GetFloatingPoint(-10.0f, 10.0f) };
                const float size{ spieler::Random::GetInstance().GetFloatingPoint(3.0f, 10.0f) };

                vertex.Position = DirectX::XMFLOAT3{ x, size / 2.0f - 0.4f, z };
                vertex.Size = DirectX::XMFLOAT2{ size, size };
            }

            // Indices
            std::array<uint32_t, treeCount> indices;
            std::iota(indices.begin(), indices.end(), 0);

            spieler::renderer::MeshGeometry& treeMeshGeometry{ m_MeshGeometries["trees"] };
            treeMeshGeometry.m_PrimitiveTopology = spieler::renderer::PrimitiveTopology::PointList;

            // VertexBuffer
            {
                const spieler::renderer::BufferConfig vertexBufferConfig
                {
                    .ElementSize = sizeof(TreeVertex),
                    .ElementCount = treeCount
                };

                const spieler::renderer::BufferFlags vertexBufferFlags{ spieler::renderer::BufferFlags::None };

                treeMeshGeometry.m_VertexBuffer.SetResource(device.CreateBuffer(vertexBufferConfig, vertexBufferFlags));
                context.CopyBuffer(vertices.data(), sizeof(TreeVertex)* vertices.size(), uploadBuffer, *treeMeshGeometry.m_VertexBuffer.GetResource());
            }

            // IndexBuffer
            {
                const spieler::renderer::BufferConfig indexBufferConfig
                {
                    .ElementSize = sizeof(uint32_t),
                    .ElementCount = treeCount
                };

                const spieler::renderer::BufferFlags indexBufferFlags{ spieler::renderer::BufferFlags::None };

                treeMeshGeometry.m_IndexBuffer.SetResource(device.CreateBuffer(indexBufferConfig, indexBufferFlags));
                context.CopyBuffer(indices.data(), sizeof(uint32_t)* indices.size(), uploadBuffer, *treeMeshGeometry.m_IndexBuffer.GetResource());
            }

            spieler::renderer::SubmeshGeometry& main{ treeMeshGeometry.CreateSubmesh("main") };
            main.IndexCount = static_cast<uint32_t>(indices.size());
            main.BaseVertexLocation = 0;
            main.StartIndexLocation = 0;
        }

        return true;
    }

    bool TestLayer::InitRootSignatures()
    {
        std::vector<spieler::renderer::RootParameter> rootParameters;
        std::vector<spieler::renderer::StaticSampler> staticSamplers;

        // Init root parameters
        {
            rootParameters.resize(4);

            rootParameters[0].Type = spieler::renderer::RootParameterType::ConstantBufferView;
            rootParameters[0].ShaderVisibility = spieler::renderer::ShaderVisibility::All;
            rootParameters[0].Child = spieler::renderer::RootDescriptor{ .ShaderRegister = 0, .RegisterSpace = 0 };

            rootParameters[1].Type = spieler::renderer::RootParameterType::ConstantBufferView;
            rootParameters[1].ShaderVisibility = spieler::renderer::ShaderVisibility::All;
            rootParameters[1].Child = spieler::renderer::RootDescriptor{ .ShaderRegister = 1, .RegisterSpace = 0 };

            rootParameters[2].Type = spieler::renderer::RootParameterType::ConstantBufferView;
            rootParameters[2].ShaderVisibility = spieler::renderer::ShaderVisibility::All;
            rootParameters[2].Child = spieler::renderer::RootDescriptor{ .ShaderRegister = 2, .RegisterSpace = 0 };

            rootParameters[3].Type = spieler::renderer::RootParameterType::DescriptorTable;
            rootParameters[3].ShaderVisibility = spieler::renderer::ShaderVisibility::All;
            rootParameters[3].Child = spieler::renderer::RootDescriptorTable{ { spieler::renderer::DescriptorRange{ spieler::renderer::DescriptorRangeType::SRV, 1 } } };
        }

        // Init statis samplers
        {
            staticSamplers.resize(6);

            staticSamplers[0] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Wrap, 0);
            staticSamplers[1] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Point, spieler::renderer::TextureAddressMode::Clamp, 1);
            staticSamplers[2] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 2);
            staticSamplers[3] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Clamp, 3);
            staticSamplers[4] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 4);
            staticSamplers[5] = spieler::renderer::StaticSampler(spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Clamp, 5);
        }

        SPIELER_RETURN_IF_FAILED(m_RootSignatures["default"].Init(rootParameters, staticSamplers));

        return true;
    }

    bool TestLayer::InitPipelineStates()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };

        const spieler::renderer::InputLayout inputLayout
        {
            spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "Normal", spieler::renderer::GraphicsFormat::R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "Tangent", spieler::renderer::GraphicsFormat::R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
            spieler::renderer::InputLayoutElement{ "TexCoord", spieler::renderer::GraphicsFormat::R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) }
        };

        // PSO for lights
        {
            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(spieler::renderer::ShaderPermutation<per::Color>{}) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(spieler::renderer::ShaderPermutation<per::Color>{}) };

            const spieler::renderer::RasterizerState rasterzerState
            {
                .FillMode = spieler::renderer::FillMode::Wireframe,
                .CullMode = spieler::renderer::CullMode::None
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .RasterizerState = &rasterzerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat(),
            };

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["light"].Init(pipelineStateConfig));
        }

        // PSO for opaque objects
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(permutation) };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::RenderTargetBlendProps renderTargetBlendProps
            {
                .Type = spieler::renderer::RenderTargetBlendingType::Default,
                .ColorEquation = spieler::renderer::BlendColorEquation
                {
                    .SourceFactor = spieler::renderer::BlendColorFactor::SourceAlpha,
                    .DestinationFactor = spieler::renderer::BlendColorFactor::InverseSourceAlpha,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .AlphaEquation = spieler::renderer::BlendAlphaEquation
                {
                    .SourceFactor = spieler::renderer::BlendAlphaFactor::One,
                    .DestinationFactor = spieler::renderer::BlendAlphaFactor::Zero,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .Channels = spieler::renderer::BlendChannel_All
            };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetProps = { renderTargetBlendProps }
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat(),
            };

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["opaque"].Init(pipelineStateConfig));
        }

        // PSO for marking mirros
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(permutation) };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetProps = 
                { 
                    spieler::renderer::RenderTargetBlendProps
                    {
                        .Channels = spieler::renderer::BlendChannel_None
                    }
                }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Disabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Replace,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Always
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Replace,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Always
                    }
                }
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat(),
            };

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["mirror"].Init(pipelineStateConfig));
        }

        // PSO for stencil reflections
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(permutation) };

            const spieler::renderer::RenderTargetBlendProps renderTargetBlendProps
            {
                .Type = spieler::renderer::RenderTargetBlendingType::Default,
                .ColorEquation = spieler::renderer::BlendColorEquation
                {
                    .SourceFactor = spieler::renderer::BlendColorFactor::SourceAlpha,
                    .DestinationFactor = spieler::renderer::BlendColorFactor::InverseSourceAlpha,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .AlphaEquation = spieler::renderer::BlendAlphaEquation
                {
                    .SourceFactor = spieler::renderer::BlendAlphaFactor::One,
                    .DestinationFactor = spieler::renderer::BlendAlphaFactor::Zero,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .Channels = spieler::renderer::BlendChannel_All
            };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetProps = { renderTargetBlendProps }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
                .TriangleOrder = spieler::renderer::TriangleOrder::Counterclockwise
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Enabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Keep,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Keep,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    }
                }
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat(),
            };

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["reflected"].Init(pipelineStateConfig));
        }

        // PSO for planar shadows
        {
            spieler::renderer::ShaderPermutation<per::Texture> permutation;
            permutation.Set<per::Texture::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Texture::USE_ALPHA_TEST>(true);
            permutation.Set<per::Texture::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(permutation) };

            const spieler::renderer::RenderTargetBlendProps renderTargetBlendProps
            {
                .Type = spieler::renderer::RenderTargetBlendingType::Default,
                .ColorEquation = spieler::renderer::BlendColorEquation
                {
                    .SourceFactor = spieler::renderer::BlendColorFactor::SourceAlpha,
                    .DestinationFactor = spieler::renderer::BlendColorFactor::InverseSourceAlpha,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .AlphaEquation = spieler::renderer::BlendAlphaEquation
                {
                    .SourceFactor = spieler::renderer::BlendAlphaFactor::One,
                    .DestinationFactor = spieler::renderer::BlendAlphaFactor::Zero,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .Channels = spieler::renderer::BlendChannel_All
            };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetProps = { renderTargetBlendProps }
            };

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::DepthStencilState depthStencilState
            {
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest = spieler::renderer::DepthTest::Enabled,
                    .DepthWriteState = spieler::renderer::DepthWriteState::Enabled,
                    .ComparisonFunction = spieler::renderer::ComparisonFunction::Less
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest = spieler::renderer::StencilTest::Enabled,
                    .ReadMask = 0xff,
                    .WriteMask = 0xff,
                    .FrontFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Increase,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    },
                    .BackFace = spieler::renderer::StencilBehaviour
                    {
                        .StencilFailOperation = spieler::renderer::StencilOperation::Keep,
                        .DepthFailOperation = spieler::renderer::StencilOperation::Keep,
                        .PassOperation = spieler::renderer::StencilOperation::Increase,
                        .StencilFunction = spieler::renderer::ComparisonFunction::Equal
                    }
                }
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .DepthStecilState = &depthStencilState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Triangle,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat(),
            };

            SPIELER_RETURN_IF_FAILED(m_PipelineStates["shadow"].Init(pipelineStateConfig));
        }

        // PSO for billboard technique
        {
            spieler::renderer::ShaderPermutation<per::Billboard> permutation;
            permutation.Set<per::Billboard::DIRECTIONAL_LIGHT_COUNT>(1);
            permutation.Set<per::Billboard::USE_ALPHA_TEST>(true);
            permutation.Set<per::Billboard::ENABLE_FOG>(true);

            const spieler::renderer::Shader& vertexShader{ GetShader<spieler::renderer::ShaderType::Vertex>(permutation) };
            const spieler::renderer::Shader& pixelShader{ GetShader<spieler::renderer::ShaderType::Pixel>(permutation) };
            const spieler::renderer::Shader& geometryShader{ GetShader<spieler::renderer::ShaderType::Geometry>(permutation) };

            // Blend state
            const spieler::renderer::RenderTargetBlendProps renderTargetBlendProps
            {
                .Type = spieler::renderer::RenderTargetBlendingType::Default,
                .ColorEquation = spieler::renderer::BlendColorEquation
                {
                    .SourceFactor = spieler::renderer::BlendColorFactor::SourceAlpha,
                    .DestinationFactor = spieler::renderer::BlendColorFactor::InverseSourceAlpha,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .AlphaEquation = spieler::renderer::BlendAlphaEquation
                {
                    .SourceFactor = spieler::renderer::BlendAlphaFactor::One,
                    .DestinationFactor = spieler::renderer::BlendAlphaFactor::Zero,
                    .Operation = spieler::renderer::BlendOperation::Add
                },
                .Channels = spieler::renderer::BlendChannel_All
            };

            const spieler::renderer::BlendState blendState
            {
                .RenderTargetProps = { renderTargetBlendProps }
            };

            // Rasterizer state
            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode = spieler::renderer::FillMode::Solid,
                .CullMode = spieler::renderer::CullMode::None,
            };

            const spieler::renderer::InputLayout inputLayout
            {
                spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32_FLOAT, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Size", spieler::renderer::GraphicsFormat::R32G32_FLOAT, sizeof(DirectX::XMFLOAT2) },
            };

            const spieler::renderer::PipelineStateConfig pipelineStateConfig
            {
                .RootSignature = &m_RootSignatures["default"],
                .VertexShader = &vertexShader,
                .PixelShader = &pixelShader,
                .GeometryShader = &geometryShader,
                .BlendState = &blendState,
                .RasterizerState = &rasterizerState,
                .InputLayout = &inputLayout,
                .PrimitiveTopologyType = spieler::renderer::PrimitiveTopologyType::Point,
                .RTVFormat = renderer.GetSwapChain().GetBufferFormat(),
                .DSVFormat = renderer.GetSwapChain().GetDepthStencilFormat()
            };

            m_PipelineStates["billboard"].Init(pipelineStateConfig);
        }

        return true;
    }

    void TestLayer::InitRenderItems()
    {
        spieler::renderer::MeshGeometry& basicMeshGeometry{ m_MeshGeometries["basic"] };

        spieler::renderer::ConstantBuffer& renderItemConstantBuffer{ m_ConstantBuffers["render_item"] };
        spieler::renderer::ConstantBuffer& colorRenderItemConstantBuffer{ m_ConstantBuffers["color_render_item"] };

        // Box
        {
            std::unique_ptr<spieler::renderer::RenderItem>& box{ m_RenderItems["box"] = std::make_unique<spieler::renderer::RenderItem>() };
            box->MeshGeometry = &basicMeshGeometry;
            box->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("box");
            box->Material = &m_Materials["wire_fence"];

            box->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 20.0f };

            renderItemConstantBuffer.SetSlice(box.get());

            m_OpaqueObjects.push_back(box.get());
        }

        // Sun
        {
            std::unique_ptr<spieler::renderer::RenderItem>& sun{ m_RenderItems["sun"] = std::make_unique<spieler::renderer::RenderItem>() };
            sun->MeshGeometry = &basicMeshGeometry;
            sun->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("geosphere");

            sun->GetComponent<ColorConstants>().Color = DirectX::XMFLOAT4{ 1.0f, 1.0f, 0.2f, 1.0f };

            colorRenderItemConstantBuffer.SetSlice(sun.get());

            m_Lights.push_back(sun.get());
        }

        // Floor
        {
            std::unique_ptr<spieler::renderer::RenderItem>& floor{ m_RenderItems["floor"] = std::make_unique<spieler::renderer::RenderItem>() };
            floor->MeshGeometry = &basicMeshGeometry;
            floor->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            floor->Material = &m_Materials["tile"];

            floor->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 20.0f };
            floor->Transform.Rotation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };
            floor->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 0.0f, 0.0f };

            floor->TextureTransform.Scale = DirectX::XMFLOAT3{ 10.0f, 10.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(floor.get());

            m_OpaqueObjects.push_back(floor.get());
        }

        // Wall 1
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall1"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ 7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 2
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall2"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ DirectX::XM_PIDIV2, DirectX::XM_PIDIV2, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -10.0f, 5.0f, 0.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 4.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Wall 3
        {
            std::unique_ptr<spieler::renderer::RenderItem>& wall{ m_RenderItems["wall3"] = std::make_unique<spieler::renderer::RenderItem>() };
            wall->MeshGeometry = &basicMeshGeometry;
            wall->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            wall->Material = &m_Materials["bricks"];

            wall->Transform.Scale = DirectX::XMFLOAT3{ 5.0f, 1.0f, 10.0f };
            wall->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            wall->Transform.Translation = DirectX::XMFLOAT3{ -7.5f, 5.0f, 10.0f };

            wall->TextureTransform.Scale = DirectX::XMFLOAT3{ 1.0f, 2.0f, 1.0f };

            renderItemConstantBuffer.SetSlice(wall.get());

            m_OpaqueObjects.push_back(wall.get());
        }

        // Skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            skull->Material = &m_Materials["skull"];

            skull->Transform.Scale = DirectX::XMFLOAT3{ 0.5f, 0.5f, 0.5f };
            skull->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 6.0f, 0.0f };

            renderItemConstantBuffer.SetSlice(skull.get());

            m_OpaqueObjects.push_back(skull.get());
        }

        // Reflected skull
        {
            std::unique_ptr<spieler::renderer::RenderItem>& skull{ m_RenderItems["reflected_skull"] = std::make_unique<spieler::renderer::RenderItem>() };
            skull->MeshGeometry = &m_MeshGeometries["skull"];
            skull->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            skull->Material = &m_Materials["skull"];

            renderItemConstantBuffer.SetSlice(skull.get());

            m_ReflectedObjects.push_back(skull.get());
        }

        // Mirror
        {
            std::unique_ptr<spieler::renderer::RenderItem>& mirror{ m_RenderItems["mirror"] = std::make_unique<spieler::renderer::RenderItem>() };
            mirror->MeshGeometry = &basicMeshGeometry;
            mirror->SubmeshGeometry = &basicMeshGeometry.GetSubmesh("grid");
            mirror->Material = &m_Materials["mirror"];

            mirror->Transform.Scale = DirectX::XMFLOAT3{ 10.0f, 1.0f, 10.0f };
            mirror->Transform.Rotation = DirectX::XMFLOAT3{ -DirectX::XM_PIDIV2, 0.0f, 0.0f };
            mirror->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, 5.0f, 10.0f };

            mirror->GetComponent<MirrorConstants>().Plane = DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, -10.0f };

            renderItemConstantBuffer.SetSlice(mirror.get());

            m_Mirrors.push_back(mirror.get());
            m_TransparentObjects.push_back(mirror.get());
        }

        // Skull shadow
        {
            std::unique_ptr<spieler::renderer::RenderItem>& shadow{ m_RenderItems["skull_shadow"] = std::make_unique<spieler::renderer::RenderItem>() };
            shadow->MeshGeometry = &m_MeshGeometries["skull"];
            shadow->SubmeshGeometry = &m_MeshGeometries["skull"].GetSubmesh("grid");
            shadow->Material = &m_Materials["shadow"];

            renderItemConstantBuffer.SetSlice(shadow.get());

            m_Shadows.push_back(shadow.get());
        }

        // Billboards
        {
            std::unique_ptr<spieler::renderer::RenderItem>& trees{ m_RenderItems["trees"] = std::make_unique<spieler::renderer::RenderItem>() };
            trees->MeshGeometry = &m_MeshGeometries["trees"];
            trees->SubmeshGeometry = &m_MeshGeometries["trees"].GetSubmesh("main");
            trees->Material = &m_Materials["tree"];

            m_AlphaTestedBillboards.push_back(trees.get());
        }
    }

    void TestLayer::InitLights()
    {
        spieler::renderer::LightConstants constants;
        constants.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };

        m_DirectionalLightController.SetConstants(&m_PassConstants["direct"].Lights[0]);
        m_DirectionalLightController.SetShape(m_RenderItems["sun"].get());
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

    void TestLayer::Render(
        std::vector<const spieler::renderer::RenderItem*> items,
        const spieler::renderer::PipelineState& pso,
        const PassConstants& passConstants,
        const spieler::renderer::ConstantBuffer* objectConstantBuffer)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };
        auto& descriptorManager{ device.GetDescriptorManager() };

        context.GetNativeCommandList()->SetPipelineState(static_cast<ID3D12PipelineState*>(pso));

        m_RootSignatures["default"].Bind();
        m_ConstantBuffers["pass"].GetSlice(&passConstants).Bind(context, 0);

        for (const spieler::renderer::RenderItem* item : items)
        {
            item->MeshGeometry->GetVertexBuffer().GetView().Bind(context);
            item->MeshGeometry->GetIndexBuffer().GetView().Bind(context);

            context.SetPrimitiveTopology(item->MeshGeometry->m_PrimitiveTopology);

            if (objectConstantBuffer)
            {
                objectConstantBuffer->GetSlice(item).Bind(context, 1);
            }

            // Material
            if (item->Material)
            {
                descriptorManager.Bind(context, spieler::renderer::DescriptorHeapType::SRV);

                m_ConstantBuffers["material"].GetSlice(item->Material).Bind(context, 2);
                item->Material->DiffuseMap.Bind(context, 3);
            }

            const spieler::renderer::SubmeshGeometry& submeshGeometry{ *item->SubmeshGeometry };
            context.GetNativeCommandList()->DrawIndexedInstanced(
                submeshGeometry.IndexCount,
                1,
                submeshGeometry.StartIndexLocation,
                submeshGeometry.BaseVertexLocation,
                0
            );
        }
    }

    bool TestLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        UpdateViewport();
        UpdateScissorRect();

        return true;
    }

} // namespace sandbox
