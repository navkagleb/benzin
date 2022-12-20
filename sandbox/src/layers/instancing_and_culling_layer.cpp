#include "bootstrap.hpp"

#include "instancing_and_culling_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/graphics/depth_stencil_state.hpp>
#include <spieler/graphics/mapped_data.hpp>
#include <spieler/graphics/rasterizer_state.hpp>
#include <spieler/graphics/resource_barrier.hpp>

#include <spieler/engine/geometry_generator.hpp>

#include <spieler/utility/random.hpp>

#include "techniques/picking_technique.hpp"

namespace sandbox
{

    namespace cb
    {

        struct Pass
        {
            alignas(16) DirectX::XMMATRIX View{};
            alignas(16) DirectX::XMMATRIX Projection{};
            alignas(16) DirectX::XMMATRIX ViewProjection{};
            alignas(16) DirectX::XMFLOAT3 CameraPosition{};
            alignas(16) DirectX::XMFLOAT4 AmbientLight{};
            alignas(16) std::array<spieler::Light, 16> Lights;
        };

        struct Entity
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            uint32_t MaterialIndex{ 0 };
        };

        struct Material
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
            uint32_t NormalMapIndex{ 0 };
        };

    } // namespace cb

    namespace per
    {

        enum class InstancingAndCulling
        {
            USE_LIGHTING,
            DIRECTIONAL_LIGHT_COUNT,
            POINT_LIGHT_COUNT,
        };

    } // namespace per

    //////////////////////////////////////////////////////////////////////////
    /// DynamicCubeMap
    //////////////////////////////////////////////////////////////////////////
    DynamicCubeMap::DynamicCubeMap(spieler::Device& device, uint32_t width, uint32_t height)
    {
        OnResize(device, width, height);
    }

    void DynamicCubeMap::SetPosition(const DirectX::XMVECTOR& position)
    {
        m_Position = position;

        for (auto& camera : m_Cameras)
        {
            camera.SetPosition(position);
        }
    }

    void DynamicCubeMap::OnResize(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitCubeMap(device, width, height);
        InitDepthStencil(device, width, height);
        InitCameras(width, height);
        InitViewport(static_cast<float>(width), static_cast<float>(height));
    }

    void DynamicCubeMap::InitCubeMap(spieler::Device& device, uint32_t width, uint32_t height)
    {
        // Resource
        {
            const spieler::TextureResource::Config config
            {
                .Type{ spieler::TextureResource::Type::_2D },
                .Width{ width },
                .Height{ height },
                .ArraySize{ 6 },
                .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
                .UsageFlags{ spieler::TextureResource::UsageFlags::RenderTarget }
            };

            const spieler::TextureResource::ClearColor clearColor
            {
                .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
            };

            m_CubeMap.SetTextureResource(spieler::TextureResource::Create(device, config, clearColor));
        }
        
        // SRV
        {
            const spieler::TextureShaderResourceView::Config srvConfig
            {
                .Type{ spieler::TextureResource::Type::Cube }
            };

            m_CubeMap.PushView<spieler::TextureShaderResourceView>(device, srvConfig);
        }
        
        // RTV
        {
            for (uint32_t i = 0; i < 6; ++i)
            {
                const spieler::TextureViewConfig rtvConfig
                {
                    .FirstArraySlice{ i },
                    .ArraySize{ 1 }
                };

                m_CubeMap.PushView<spieler::TextureRenderTargetView>(device, rtvConfig);
            }
        }
    }

    void DynamicCubeMap::InitDepthStencil(spieler::Device& device, uint32_t width, uint32_t height)
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::_2D },
            .Width{ width },
            .Height{ height },
            .ArraySize{ 1 },
            .Format{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt },
            .UsageFlags{ spieler::TextureResource::UsageFlags::DepthStencil }
        };

        const spieler::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        m_DepthStencil.SetTextureResource(spieler::TextureResource::Create(device, config, clearDepthStencil));
        m_DepthStencil.PushView<spieler::TextureDepthStencilView>(device);
    }

    void DynamicCubeMap::InitCameras(uint32_t width, uint32_t height)
    {
        const std::array<DirectX::XMVECTOR, 6> frontDirections
        {
            DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f, 0.0f },    // +X
            DirectX::XMVECTOR{ -1.0f, 0.0f, 0.0f, 0.0f },   // -X
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +Y
            DirectX::XMVECTOR{ 0.0f, -1.0f, 0.0f, 0.0f },   // -Y
            DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f },    // +Z
            DirectX::XMVECTOR{ 0.0f, 0.0f, -1.0f, 0.0f }    // -Z
        };

        const std::array<DirectX::XMVECTOR, 6> upDirections
        {
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +X
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // -X
            DirectX::XMVECTOR{ 0.0f, 0.0f, -1.0f, 0.0f },   // +Y
            DirectX::XMVECTOR{ 0.0f, 0.0f, 1.0f, 0.0f },    // -Y
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f },    // +Z
            DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f, 0.0f }     // -Z
        };

        for (size_t i = 0; i < m_Cameras.size(); ++i)
        {
            m_Cameras[i].SetPosition(m_Position);
            m_Cameras[i].SetFrontDirection(frontDirections[i]);
            m_Cameras[i].SetUpDirection(upDirections[i]);

            m_Cameras[i].SetAspectRatio(1.0f);
            m_Cameras[i].SetFOV(DirectX::XMConvertToRadians(90.0f));
            m_Cameras[i].SetNearPlane(0.1f);
            m_Cameras[i].SetFarPlane(1000.0f);
        }
    }

    void DynamicCubeMap::InitViewport(float width, float height)
    {
        m_Viewport.Width = width;
        m_Viewport.Height = height;

        m_ScissorRect.Width = width;
        m_ScissorRect.Height = height;
    }

    //////////////////////////////////////////////////////////////////////////
    //// PointLightController
    //////////////////////////////////////////////////////////////////////////
    InstancingAndCullingLayer::PointLightController::PointLightController(spieler::Light* light)
        : m_Light{ light }
    {}

    void InstancingAndCullingLayer::PointLightController::OnImGuiRender()
    {
        SPIELER_ASSERT(m_Light);

        ImGui::Begin("Point Light Controller");
        {
            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&m_Light->Position), 0.1f);
            ImGui::ColorEdit3("Strength", reinterpret_cast<float*>(&m_Light->Strength));

            ImGui::DragFloat("Falloff Start", &m_Light->FalloffStart, 0.1f, 0.0f, m_Light->FalloffEnd);

            if (ImGui::DragFloat("Falloff End", &m_Light->FalloffEnd, 0.1f, 0.0f))
            {
                if (m_Light->FalloffStart > m_Light->FalloffEnd)
                {
                    m_Light->FalloffStart = m_Light->FalloffEnd;
                }
            }
        }
        ImGui::End();
    }

    struct PickedEntityComponent
    {
        uint32_t InstanceIndex{ 0 };
        uint32_t TriangleIndex{ 0 };
    };

    //////////////////////////////////////////////////////////////////////////
    /// InstancingAndCullingLayer
    //////////////////////////////////////////////////////////////////////////
    InstancingAndCullingLayer::InstancingAndCullingLayer(
        spieler::Window& window, 
        spieler::Device& device, 
        spieler::CommandQueue& commandQueue, 
        spieler::SwapChain& swapChain
    )
        : m_Window{ window }
        , m_Device{ device }
        , m_CommandQueue{ commandQueue }
        , m_SwapChain{ swapChain }
        , m_GraphicsCommandList{ device }
        , m_PostEffectsGraphicsCommandList{ device }
    {}

    bool InstancingAndCullingLayer::OnAttach()
    {
        //application.GetImGuiLayer()->SetCamera(&m_Camera);

        {
            m_GraphicsCommandList.Reset();

            InitTextures();
            InitMesh();

            m_GraphicsCommandList.Close();

            m_CommandQueue.Submit(m_GraphicsCommandList);
            m_CommandQueue.Flush();
        }

        m_DynamicCubeMap = std::make_unique<DynamicCubeMap>(m_Device, 200, 200);
        m_BlurPass = std::make_unique<BlurPass>(m_Device, m_Window.GetWidth(), m_Window.GetHeight());

        InitBuffers();

        // Init m_Materials
        {
            // Red
            {
                m_Materials["red"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 0 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 0 }
                };
            }

            // Green
            {
                m_Materials["green"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.7f },
                    .DiffuseMapIndex{ 1 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 1 }
                };
            }

            // Blue
            {
                m_Materials["blue"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 2 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 2 }
                };
            }

            // White
            {
                m_Materials["white"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 3 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 3 }
                };
            }

            // Environment
            {
                m_Materials["environment"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.1f, 0.1f, 0.1f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 4 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 4 }
                };
            }

            // Mirror
            {
                m_Materials["mirror"] = spieler::Material
                {
                    .DiffuseAlbedo{ 0.0f, 0.0f, 0.0f, 1.0f },
                    .FresnelR0{ 0.98f, 0.97f, 0.95f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 5 },
                    .NormalMapIndex{ 7 },
                    .StructuredBufferIndex{ 5 }
                };
            }

            // Bricks2
            {
                m_Materials["bricks2"] = spieler::Material
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.9f },
                    .DiffuseMapIndex{ 5 },
                    .NormalMapIndex{ 6 },
                    .StructuredBufferIndex{ 6 }
                };
            }
        }

        // Init m_RenderItems
        {
            // Environment CubeMap
            {
                auto& entity = m_Entities["environment"];
                entity = std::make_unique<spieler::Entity>();

                auto& meshComponent = entity->CreateComponent<spieler::MeshComponent>();
                meshComponent.Mesh = &m_Mesh;
                meshComponent.SubMesh = &m_Mesh.GetSubMesh("box");
                meshComponent.PrimitiveTopology = spieler::PrimitiveTopology::TriangleList;

                auto& instancesComponent = entity->CreateComponent<spieler::InstancesComponent>();
                auto& instanceComponent = instancesComponent.Instances.emplace_back();
                instanceComponent.MaterialIndex = 4;
                instanceComponent.Transform = spieler::Transform
                {
                    .Scale{ 100.0f, 100.0f, 100.0f },
                    .Translation{ 2.0f, 0.0f, 0.0f }
                };
            }

            // Dynamic sphere
            {
                auto& entity = m_Entities["dynamic_sphere"];
                entity = std::make_unique<spieler::Entity>();

                auto& meshComponent = entity->CreateComponent<spieler::MeshComponent>();
                meshComponent.Mesh = &m_Mesh;
                meshComponent.SubMesh = &m_Mesh.GetSubMesh("sphere");
                meshComponent.PrimitiveTopology = spieler::PrimitiveTopology::TriangleList;

                auto& collisionComponent = entity->CreateComponent<spieler::CollisionComponent>();
                collisionComponent.CreateBoundingSphere(meshComponent);

                auto& instancesComponent = entity->CreateComponent<spieler::InstancesComponent>();
                instancesComponent.IsNeedCulling = true;

                auto& instanceComponent = instancesComponent.Instances.emplace_back();
                instanceComponent.MaterialIndex = 5;
                instanceComponent.Transform = spieler::Transform
                {
                    .Scale{ 2.5f, 2.5f, 2.5f },
                    .Translation{ -5.0f, 0.0f, 0.0f }
                };

                m_DynamicCubeMap->SetPosition({ -5.0f, 0.0f, 0.0f, 1.0f });

                m_PickableEntities.push_back(entity.get());
            }

            // Boxes
            {
                const int32_t boxInRowCount = 10;
                const int32_t boxInColumnCount = 10;
                const int32_t boxInDepthCount = 10;

                auto& entity = m_Entities["box"];
                entity = std::make_unique<spieler::Entity>();

                auto& meshComponent = entity->CreateComponent<spieler::MeshComponent>();
                meshComponent.Mesh = &m_Mesh;
                meshComponent.SubMesh = &m_Mesh.GetSubMesh("box");
                meshComponent.PrimitiveTopology = spieler::PrimitiveTopology::TriangleList;
                
                auto& collisionComponent = entity->CreateComponent<spieler::CollisionComponent>();
                collisionComponent.CreateBoundingBox(meshComponent);

                auto& instancesComponent = entity->CreateComponent<spieler::InstancesComponent>();
                instancesComponent.IsNeedCulling = true;
                instancesComponent.Instances.resize(boxInRowCount * boxInColumnCount * boxInDepthCount);

                for (int32_t i = 0; i < boxInRowCount; ++i)
                {
                    for (int32_t j = 0; j < boxInColumnCount; ++j)
                    {
                        for (int32_t k = 0; k < boxInDepthCount; ++k)
                        {
                            const auto index = static_cast<size_t>((i * boxInRowCount + j) * boxInColumnCount + k);

                            auto& instanceComponent = instancesComponent.Instances[index];
                            instanceComponent.Transform.Translation = DirectX::XMFLOAT3{ static_cast<float>((i - 5) * 10), static_cast<float>((j - 5) * 10), static_cast<float>((k - 5) * 10) };
                            instanceComponent.Transform.Scale = DirectX::XMFLOAT3{ 2.0f, 2.0f, 2.0f };
                            instanceComponent.MaterialIndex = 6;
                        }
                    }
                }

                m_DefaultEntities.push_back(entity.get());
                m_PickableEntities.push_back(entity.get());
            }

            // Picked
            {
                auto& entity = m_Entities["picked"];
                entity = std::make_unique<spieler::Entity>();

                auto& meshComponent = entity->CreateComponent<spieler::MeshComponent>();
                meshComponent.Mesh = &m_Mesh;
                meshComponent.PrimitiveTopology = spieler::PrimitiveTopology::TriangleList;

                auto& instancesComponent = entity->CreateComponent<spieler::InstancesComponent>();
                instancesComponent.IsNeedCulling = true;

                auto& instanceComponent = instancesComponent.Instances.emplace_back();
                instanceComponent.MaterialIndex = 0;
                instanceComponent.IsVisible = false;

                entity->CreateComponent<PickedEntityComponent>();
            }

            // Light source
            {
                auto& entity = m_Entities["light"];
                entity = std::make_unique<spieler::Entity>();

                auto& meshComponent = entity->CreateComponent<spieler::MeshComponent>();
                meshComponent.Mesh = &m_Mesh;
                meshComponent.SubMesh = &m_Mesh.GetSubMesh("box");
                meshComponent.PrimitiveTopology = spieler::PrimitiveTopology::TriangleList;

                auto& collisionComponent = entity->CreateComponent<spieler::CollisionComponent>();
                collisionComponent.CreateBoundingBox(meshComponent);

                auto& instancesComponent = entity->CreateComponent<spieler::InstancesComponent>();
                instancesComponent.IsNeedCulling = true;

                const DirectX::XMFLOAT3 position{ 0.0f, 3.0f, 0.0f };

                auto& instanceComponent = instancesComponent.Instances.emplace_back();
                instanceComponent.MaterialIndex = 3;
                instanceComponent.Transform.Translation = position;

                auto& lightComponent = entity->CreateComponent<spieler::Light>();
                lightComponent.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };
                lightComponent.FalloffStart = 20.0f;
                lightComponent.FalloffEnd = 23.0f;
                lightComponent.Position = position;

                m_LightSourceEntities.push_back(entity.get());
                m_PickableEntities.push_back(entity.get());

                m_PointLightController = PointLightController{ &lightComponent };
            }
        }

        InitRootSignature();
        InitPipelineState();

        OnWindowResized();

        m_PickingTechnique = std::make_unique<PickingTechnique>(m_Window);
        m_PickingTechnique->SetActiveCamera(&m_Camera);
        m_PickingTechnique->SetPickableEntities(m_PickableEntities);

        return true;
    }

    bool InstancingAndCullingLayer::OnDetach()
    {
        m_CommandQueue.Flush();

        return true;
    }

    void InstancingAndCullingLayer::OnEvent(spieler::Event& event)
    {
        m_CameraController.OnEvent(event);

        spieler::EventDispatcher dispatcher{ event };

        dispatcher.Dispatch<spieler::WindowResizedEvent>([this](spieler::WindowResizedEvent& event)
        {
            OnWindowResized();

            return false;
        });

        dispatcher.Dispatch<spieler::MouseButtonPressedEvent>([this](spieler::MouseButtonPressedEvent& event)
        {
            if (event.GetButton() == spieler::MouseButton::Right)
            {
                const PickingTechnique::Result result = m_PickingTechnique->PickTriangle(event.GetX<float>(), event.GetY<float>());
                const spieler::Entity* resultEntity = result.PickedEntity;

                auto& pickedEntity = m_Entities["picked"];
                auto& instanceComponent = pickedEntity->GetComponent<spieler::InstancesComponent>().Instances[0];

                if (!result.PickedEntity)
                {
                    instanceComponent.IsVisible = false;
                }
                else
                {
                    auto& meshComponent = pickedEntity->GetComponent<spieler::MeshComponent>();
                    meshComponent.Mesh = result.PickedEntity->GetComponent<spieler::MeshComponent>().Mesh;
                    meshComponent.SubMesh = new spieler::SubMesh
                    {
                        .VertexCount{ 3 },
                        .IndexCount{ 3 },
                        .BaseVertexLocation{ resultEntity->GetComponent<spieler::MeshComponent>().SubMesh->BaseVertexLocation },
                        .StartIndexLocation{ resultEntity->GetComponent<spieler::MeshComponent>().SubMesh->StartIndexLocation + result.TriangleIndex * 3 }
                    };

                    auto& collisionComponent = pickedEntity->GetOrCreateComponent<spieler::CollisionComponent>();
                    collisionComponent.CreateBoundingBox(meshComponent);

                    instanceComponent.IsVisible = true;
                    instanceComponent.Transform = resultEntity->GetComponent<spieler::InstancesComponent>().Instances[result.InstanceIndex].Transform;
                
                    auto& pickedEntityComponent = pickedEntity->GetComponent<PickedEntityComponent>();
                    pickedEntityComponent.InstanceIndex = result.InstanceIndex;
                    pickedEntityComponent.TriangleIndex = result.TriangleIndex;
                }
            }

            return false;
        });
    }

    void InstancingAndCullingLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Pass ConstantBuffer
        {
            spieler::MappedData bufferData{ m_Buffers["pass_constant_buffer"].GetBufferResource(), 0 };
            
            // Main pass
            {
                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(m_Camera.GetView()) },
                    .Projection{ DirectX::XMMatrixTranspose(m_Camera.GetProjection()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(m_Camera.GetViewProjection()) },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight }
                };

                const uint64_t lightSize = sizeof(spieler::Light);

                for (size_t i = 0; i < m_LightSourceEntities.size(); ++i)
                {
                    const auto& lightComponent = m_LightSourceEntities[i]->GetComponent<spieler::Light>();
                    memcpy_s(constants.Lights.data() + lightSize * i, lightSize, &lightComponent, lightSize);
                }

                bufferData.Write(&constants, sizeof(constants));
            }
            

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const spieler::Camera& camera = m_DynamicCubeMap->GetCamera(faceIndex);

                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(camera.GetView()) },
                    .Projection{ DirectX::XMMatrixTranspose(camera.GetProjection()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(camera.GetViewProjection()) },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight }
                };

                const uint64_t lightSize = sizeof(spieler::Light);

                for (size_t i = 0; i < m_LightSourceEntities.size(); ++i)
                {
                    const auto& lightComponent = m_LightSourceEntities[i]->GetComponent<spieler::Light>();
                    memcpy_s(constants.Lights.data() + lightSize * i, lightSize, &lightComponent, lightSize);
                }

                bufferData.Write(&constants, sizeof(constants), m_Buffers["pass_constant_buffer"].GetBufferResource()->GetConfig().ElementSize * (faceIndex + 1));
            }
        }

        // RenderItem StructuredBuffer
        {
            spieler::MappedData bufferData{ m_Buffers["render_item_structured_buffer"].GetBufferResource(), 0 };

            uint32_t offset = 0;

            for (auto& [name, entity] : m_Entities)
            {
                uint32_t visibleInstanceCount = 0;

                const auto& meshComponent = entity->GetComponent<spieler::MeshComponent>();
                auto& instancesComponent = entity->GetComponent<spieler::InstancesComponent>();

                if (entity->HasComponent<spieler::Light>())
                {
                    const auto& lightComponent = entity->GetComponent<spieler::Light>();
                    auto& instanceComponent = instancesComponent.Instances[0];
                    
                    instanceComponent.Transform.Translation = lightComponent.Position;
                }

                for (const auto& instanceComponent : instancesComponent.Instances)
                {
                    if (!meshComponent.SubMesh || !instanceComponent.IsVisible)
                    {
                        continue;
                    }

                    const DirectX::XMMATRIX viewToLocal = DirectX::XMMatrixMultiply(m_Camera.GetInverseView(), instanceComponent.Transform.GetInverseMatrix());
                    const DirectX::BoundingFrustum localSpaceFrustum = m_Camera.GetTransformedBoundingFrustum(viewToLocal);

                    if (!m_IsCullingEnabled || !instancesComponent.IsNeedCulling)
                    {
                        const cb::Entity constants
                        {
                            .World{ DirectX::XMMatrixTranspose(instanceComponent.Transform.GetMatrix()) },
                            .MaterialIndex{ instanceComponent.MaterialIndex }
                        };

                        bufferData.Write(&constants, sizeof(constants), (offset + visibleInstanceCount++) * sizeof(constants));
                    }
                    else
                    {
                        const auto& collisionComponent = entity->GetComponent<spieler::CollisionComponent>();

                        if (collisionComponent.IsCollides(localSpaceFrustum))
                        {
                            const cb::Entity constants
                            {
                                .World{ DirectX::XMMatrixTranspose(instanceComponent.Transform.GetMatrix()) },
                                .MaterialIndex{ instanceComponent.MaterialIndex }
                            };

                            bufferData.Write(&constants, sizeof(constants), (offset + visibleInstanceCount++) * sizeof(constants));
                        }
                    }

#if 0
                    if (!m_IsCullingEnabled || !instancesComponent.IsNeedCulling)
                    {
                        if (
                            !entity->HasComponent<spieler::CollisionComponent>() ||
                            entity->GetComponent<spieler::CollisionComponent>().IsCollides(localSpaceFrustum)
                        )
                        {
                            const cb::Entity constants
                            {
                                .World{ DirectX::XMMatrixTranspose(instanceComponent.Transform.GetMatrix()) },
                                .MaterialIndex{ instanceComponent.MaterialIndex }
                            };

                            bufferData.Write(&constants, sizeof(constants), (offset + visibleInstanceCount++) * sizeof(constants));
                        }
                    }
#endif
                }

                instancesComponent.VisibleInstanceCount = visibleInstanceCount;
                instancesComponent.StructuredBufferOffset = offset;

                offset += visibleInstanceCount;
            }
        }

        // Material StructuredBuffer
        {
            spieler::MappedData bufferData{ m_Buffers["material_structured_buffer"].GetBufferResource(), 0 };

            for (const auto& [name, material] : m_Materials)
            {
                const cb::Material constants
                {
                    .DiffuseAlbedo{ material.DiffuseAlbedo },
                    .FresnelR0{ material.FresnelR0 },
                    .Roughness{ material.Roughness },
                    .Transform{ material.Transform },
                    .DiffuseMapIndex{ material.DiffuseMapIndex },
                    .NormalMapIndex{ material.NormalMapIndex }
                };

                bufferData.Write(&constants, sizeof(constants), material.StructuredBufferIndex * sizeof(constants));
            }
        }
    }

    void InstancingAndCullingLayer::OnRender(float dt)
    {
        auto& renderTarget{ m_Textures["render_target"] };
        auto& depthStencil{ m_Textures["depth_stencil"] };

        auto& descriptorManager{ m_Device.GetDescriptorManager() };

        {
            m_GraphicsCommandList.Reset();

            // Render to cube map
            {
                m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_DynamicCubeMap->GetCubeMap().GetTextureResource().get() },
                    .From{ spieler::ResourceState::Present },
                    .To{ spieler::ResourceState::RenderTarget }
                });

                m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_DynamicCubeMap->GetDepthStencil().GetTextureResource().get() },
                    .From{ spieler::ResourceState::Present },
                    .To{ spieler::ResourceState::DepthWrite }
                });

                m_GraphicsCommandList.SetViewport(m_DynamicCubeMap->GetViewport());
                m_GraphicsCommandList.SetScissorRect(m_DynamicCubeMap->GetScissorRect());

                m_GraphicsCommandList.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::DescriptorHeap::Type::SRV));
                m_GraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);

                for (uint32_t i = 0; i < 6; ++i)
                {
                    m_GraphicsCommandList.ClearRenderTarget(m_DynamicCubeMap->GetCubeMap().GetView<spieler::TextureRenderTargetView>(i), { 0.1f, 0.1f, 0.1f, 1.0f });
                    m_GraphicsCommandList.ClearDepthStencil(m_DynamicCubeMap->GetDepthStencil().GetView<spieler::TextureDepthStencilView>(), 1.0f, 0);

                    m_GraphicsCommandList.SetRenderTarget(
                        m_DynamicCubeMap->GetCubeMap().GetView<spieler::TextureRenderTargetView>(i),
                        m_DynamicCubeMap->GetDepthStencil().GetView<spieler::TextureDepthStencilView>()
                    );

                    m_GraphicsCommandList.SetGraphicsRawConstantBuffer(0, *m_Buffers.at("pass_constant_buffer").GetBufferResource(), i + 1);

                    m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_Textures.at("space_cubemap").GetView<spieler::TextureShaderResourceView>());
                    
                    RenderEntities(m_PipelineStates.at("default"), m_DefaultEntities);

                    const spieler::Entity* environmentEntity = m_Entities.at("environment").get();
                    RenderEntities(m_PipelineStates.at("environment"), std::span{ &environmentEntity, 1});
                }

                m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_DynamicCubeMap->GetCubeMap().GetTextureResource().get() },
                    .From{ spieler::ResourceState::RenderTarget },
                    .To{ spieler::ResourceState::Present }
                });

                m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_DynamicCubeMap->GetDepthStencil().GetTextureResource().get() },
                    .From{ spieler::ResourceState::DepthWrite },
                    .To{ spieler::ResourceState::Present }
                });
            }

            m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ renderTarget.GetTextureResource().get() },
                .From{ spieler::ResourceState::Present },
                .To{ spieler::ResourceState::RenderTarget }
            });

            m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ depthStencil.GetTextureResource().get() },
                .From{ spieler::ResourceState::Present },
                .To{ spieler::ResourceState::DepthWrite }
            });

            m_GraphicsCommandList.SetViewport(m_Window.GetViewport());
            m_GraphicsCommandList.SetScissorRect(m_Window.GetScissorRect());

            m_GraphicsCommandList.SetRenderTarget(renderTarget.GetView<spieler::TextureRenderTargetView>(), depthStencil.GetView<spieler::TextureDepthStencilView>());

            m_GraphicsCommandList.ClearRenderTarget(renderTarget.GetView<spieler::TextureRenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            m_GraphicsCommandList.ClearDepthStencil(depthStencil.GetView<spieler::TextureDepthStencilView>(), 1.0f, 0);

            m_GraphicsCommandList.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::DescriptorHeap::Type::SRV));
            m_GraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);

            m_GraphicsCommandList.SetGraphicsRawConstantBuffer(0, *m_Buffers.at("pass_constant_buffer").GetBufferResource());

            m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_DynamicCubeMap->GetCubeMap().GetView<spieler::TextureShaderResourceView>());
            
            const spieler::Entity* dynamicSphereEntity = m_Entities.at("dynamic_sphere").get();
            RenderEntities(m_PipelineStates.at("default"), std::span{ &dynamicSphereEntity, 1 });

            m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_Textures.at("space_cubemap").GetView<spieler::TextureShaderResourceView>());
            
            RenderEntities(m_PipelineStates.at("default"), m_DefaultEntities);
            
            const spieler::Entity* pickedEntity = m_Entities.at("picked").get();
            RenderEntities(m_PipelineStates.at("picked"), std::span{ &pickedEntity, 1 });

            RenderEntities(m_PipelineStates.at("lighting"), m_LightSourceEntities);

            const spieler::Entity* environmentEntity = m_Entities.at("environment").get();
            RenderEntities(m_PipelineStates.at("environment"), std::span{ &environmentEntity, 1});

            m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ renderTarget.GetTextureResource().get() },
                .From{ spieler::ResourceState::RenderTarget },
                .To{ spieler::ResourceState::Present }
            });

            m_GraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
            {
                .Resource{ depthStencil.GetTextureResource().get() },
                .From{ spieler::ResourceState::DepthWrite },
                .To{ spieler::ResourceState::Present }
            });

            m_GraphicsCommandList.Close();
            m_CommandQueue.Submit(m_GraphicsCommandList);
        }

        {
            m_PostEffectsGraphicsCommandList.Reset();

            m_PostEffectsGraphicsCommandList.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::DescriptorHeap::Type::SRV));

            m_BlurPass->OnExecute(m_PostEffectsGraphicsCommandList, *m_Textures["render_target"].GetTextureResource(), BlurPassExecuteProps{ 2.5f, 2.5f, 0 });

            RenderFullscreenQuad(m_BlurPass->GetOutput().GetView<spieler::TextureShaderResourceView>());

            m_PostEffectsGraphicsCommandList.Close();
            m_CommandQueue.Submit(m_PostEffectsGraphicsCommandList);
        }
    }

    void InstancingAndCullingLayer::OnImGuiRender(float dt)
    {
        m_PointLightController.OnImGuiRender();

        ImGui::Begin("Culling Data");
        {   
            ImGui::Checkbox("Is Culling Enabled", &m_IsCullingEnabled);

            for (const auto& [name, entity] : m_Entities)
            {
                const auto& instancesComponent = entity->GetComponent<spieler::InstancesComponent>();
                ImGui::Text("%s visible instances: %d", name.c_str(), instancesComponent.VisibleInstanceCount);
            }
        }
        ImGui::End();

        ImGui::Begin("Picked Entity");
        {
            const auto& pickedEntity = m_Entities["picked"];
            const auto& instancesComponent = pickedEntity->GetComponent<spieler::InstancesComponent>();

            if (!instancesComponent.Instances[0].IsVisible)
            {
                ImGui::Text("Null");
            }
            else
            {
                const auto& pickedEntityComponent = pickedEntity->GetComponent<PickedEntityComponent>();
                ImGui::Text("Instance Index: %d", pickedEntityComponent.InstanceIndex);
                ImGui::Text("Triangle Index: %d", pickedEntityComponent.TriangleIndex);
            }
        }
        ImGui::End();

        ImGui::Begin("DynamicCube RenderItem");
        {
            auto& dynamicSphereEntity = m_Entities["dynamic_sphere"];
            auto& instanceComponent = dynamicSphereEntity->GetComponent<spieler::InstancesComponent>().Instances[0];

            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&instanceComponent.Transform.Translation), 0.1f);
            
            m_DynamicCubeMap->SetPosition(DirectX::XMLoadFloat3(&instanceComponent.Transform.Translation));
        }
        ImGui::End();
    }

    void InstancingAndCullingLayer::InitTextures()
    {
        const auto loadTexture = [&](const std::string& name, const std::wstring& filepath)
        {
            auto& texture{ m_Textures[name] };
            texture.SetTextureResource(spieler::TextureResource::LoadFromDDSFile(m_Device, m_GraphicsCommandList, filepath));
            texture.PushView<spieler::TextureShaderResourceView>(m_Device);
        };

        loadTexture("red", L"assets/textures/red.dds");
        loadTexture("green", L"assets/textures/green.dds");
        loadTexture("blue", L"assets/textures/blue.dds");
        loadTexture("white", L"assets/textures/white.dds");
        loadTexture("space_cubemap", L"assets/textures/space_cubemap.dds");
        loadTexture("bricks2", L"assets/textures/bricks2.dds");
        loadTexture("bricks2_normalmap", L"assets/textures/bricks2_normalmap.dds");
        loadTexture("default_normalmap", L"assets/textures/default_normalmap.dds");
    }

    void InstancingAndCullingLayer::InitMesh()
    {
        const spieler::BoxGeometryConfig boxProps
        {
            .Width{ 1.0f },
            .Height{ 1.0f },
            .Depth{ 1.0f }
        };

        const spieler::SphereGeometryConfig sphereProps
        {
            .Radius{ 1.0f },
            .SliceCount{ 16 },
            .StackCount{ 16 }
        };

        const std::unordered_map<std::string, spieler::MeshData> submeshes
        {
            { "box", spieler::GeometryGenerator::GenerateBox(boxProps) },
            { "sphere", spieler::GeometryGenerator::GenerateSphere(sphereProps) }
        };

        m_Mesh.SetSubMeshes(m_Device, submeshes);

        m_GraphicsCommandList.UploadToBuffer(*m_Mesh.GetVertexBuffer().GetBufferResource(), m_Mesh.GetVertices().data(), m_Mesh.GetVertices().size() * sizeof(spieler::Vertex));
        m_GraphicsCommandList.UploadToBuffer(*m_Mesh.GetIndexBuffer().GetBufferResource(), m_Mesh.GetIndices().data(), m_Mesh.GetIndices().size() * sizeof(uint32_t));
    }

    void InstancingAndCullingLayer::InitBuffers()
    {
        // Pass ConstantBuffer
        {
            auto& buffer{ m_Buffers["pass_constant_buffer"] };

            const spieler::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Pass) },
                .ElementCount{ 7 },
                .Flags{ spieler::BufferResource::Flags::ConstantBuffer }
            };

            buffer.SetBufferResource(spieler::BufferResource::Create(m_Device, config));
        }

        // RenderItem StructuredBuffer
        {
            auto& buffer{ m_Buffers["render_item_structured_buffer"] };

            const spieler::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Entity) },
                .ElementCount{ 1004 },
                .Flags{ spieler::BufferResource::Flags::Dynamic }
            };

            buffer.SetBufferResource(spieler::BufferResource::Create(m_Device, config));
        }

        // Material StructuredBuffer
        {
            auto& buffer{ m_Buffers["material_structured_buffer"] };

            const spieler::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Material) },
                .ElementCount{ 6 },
                .Flags{ spieler::BufferResource::Flags::Dynamic }
            };

            buffer.SetBufferResource(spieler::BufferResource::Create(m_Device, config));
        }
    }

    void InstancingAndCullingLayer::InitRootSignature()
    {
        spieler::RootSignature::Config config{ 5, 2 };

        // Root parameters
        {
            const spieler::RootParameter::Descriptor passConstantBuffer
            {
                .Type{ spieler::RootParameter::DescriptorType::ConstantBufferView },
                .ShaderRegister{ 0 }
            };

            const spieler::RootParameter::Descriptor renderItemStructuredBuffer
            {
                .Type{ spieler::RootParameter::DescriptorType::ShaderResourceView },
                .ShaderRegister{ 1, 1 }
            };

            const spieler::RootParameter::Descriptor materialStructuredBuffer
            {
                .Type{ spieler::RootParameter::DescriptorType::ShaderResourceView },
                .ShaderRegister{ 1, 2 }
            };

            const spieler::RootParameter::SingleDescriptorTable cubeMap
            {
                .Range
                {
                    .Type{ spieler::RootParameter::DescriptorRangeType::ShaderResourceView },
                    .DescriptorCount{ 1 },
                    .BaseShaderRegister{ 0 }
                }
            };

            const spieler::RootParameter::SingleDescriptorTable textureTable
            {
                .Range
                {
                    .Type{ spieler::RootParameter::DescriptorRangeType::ShaderResourceView },
                    .DescriptorCount{ 10 },
                    .BaseShaderRegister{ 1 }
                }
            };

            config.RootParameters[0] = passConstantBuffer;
            config.RootParameters[1] = renderItemStructuredBuffer;
            config.RootParameters[2] = materialStructuredBuffer;
            config.RootParameters[3] = cubeMap;
            config.RootParameters[4] = textureTable;
        }

        // Static samplers
        {
            config.StaticSamplers[0] = spieler::StaticSampler{ spieler::TextureFilterType::Linear, spieler::TextureAddressMode::Wrap, 0 };
            config.StaticSamplers[1] = spieler::StaticSampler{ spieler::TextureFilterType::Anisotropic, spieler::TextureAddressMode::Wrap, 1 };
        }

        m_RootSignature = spieler::RootSignature{ m_Device, config };
    }

    void InstancingAndCullingLayer::InitPipelineState()
    {
        const spieler::Shader* vertexShader{ nullptr };
        const spieler::Shader* pixelShader{ nullptr };

        // Init Shaders
        {
            const std::wstring_view filepath{ L"assets/shaders/default.hlsl" };

            const spieler::Shader::Config vertexShaderConfig
            {
                .Type{ spieler::Shader::Type::Vertex },
                .Filepath{ filepath },
                .EntryPoint{ "VS_Main" }
            };

            const spieler::Shader::Config pixelShaderConfig
            {
                .Type{ spieler::Shader::Type::Pixel },
                .Filepath{ filepath },
                .EntryPoint{ "PS_Main" },
                .Defines
                {
                    { magic_enum::enum_name(per::InstancingAndCulling::USE_LIGHTING), "" },
                    { magic_enum::enum_name(per::InstancingAndCulling::DIRECTIONAL_LIGHT_COUNT), "0" },
                    { magic_enum::enum_name(per::InstancingAndCulling::POINT_LIGHT_COUNT), "1" },
                }
            };

            m_ShaderLibrary["default_vs"] = spieler::Shader::Create(vertexShaderConfig);
            m_ShaderLibrary["default_ps"] = spieler::Shader::Create(pixelShaderConfig);

            vertexShader = m_ShaderLibrary["default_vs"].get();
            pixelShader = m_ShaderLibrary["default_ps"].get();
        }

        const spieler::RasterizerState rasterizerState
        {
            .FillMode{ spieler::FillMode::Solid },
            .CullMode{ spieler::CullMode::None },
            .TriangleOrder{ spieler::TriangleOrder::Clockwise }
        };

        const spieler::InputLayout inputLayout
        {
            spieler::InputLayoutElement{ "Position", spieler::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::InputLayoutElement{ "Normal", spieler::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::InputLayoutElement{ "Tangent", spieler::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            spieler::InputLayoutElement{ "TexCoord", spieler::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
        };

        // Default PSO
        {
            const spieler::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::DepthState::TestState::Enabled },
                    .WriteState{ spieler::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ spieler::StencilState::TestState::Disabled },
                }
            };

            const spieler::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["default"] = spieler::GraphicsPipelineState{ m_Device, config };
        }

        // PSO for picked RenderItem
        {
            const spieler::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::DepthState::TestState::Enabled },
                    .WriteState{ spieler::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::ComparisonFunction::LessEqual }
                },
                    .StencilState
                {
                    .TestState{ spieler::StencilState::TestState::Disabled },
                }
            };

            const spieler::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["picked"] = spieler::GraphicsPipelineState{ m_Device, config };
        }

        // PSO for lighting
        {
            const spieler::Shader* vertexShader{ nullptr };
            const spieler::Shader* pixelShader{ nullptr };

            // Init Shaders
            {
                const std::wstring_view filepath{ L"assets/shaders/default.hlsl" };

                const spieler::Shader::Config pixelShaderConfig
                {
                    .Type{ spieler::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["lighting_ps"] = spieler::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["default_vs"].get();
                pixelShader = m_ShaderLibrary["lighting_ps"].get();
            }

            const spieler::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::DepthState::TestState::Enabled },
                    .WriteState{ spieler::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ spieler::StencilState::TestState::Disabled },
                }
            };

            const spieler::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["lighting"] = spieler::GraphicsPipelineState{ m_Device, config };
        }

        // PSO for CubeMap
        {
            const spieler::Shader* vertexShader{ nullptr };
            const spieler::Shader* pixelShader{ nullptr };

            // Init Shaders
            {
                const std::wstring filepath{ L"assets/shaders/environment.hlsl" };

                const spieler::Shader::Config vertexShaderConfig
                {
                    .Type{ spieler::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const spieler::Shader::Config pixelShaderConfig
                {
                    .Type{ spieler::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["environment_vs"] = spieler::Shader::Create(vertexShaderConfig);
                m_ShaderLibrary["environment_ps"] = spieler::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["environment_vs"].get();
                pixelShader = m_ShaderLibrary["environment_ps"].get();
            }

            const spieler::RasterizerState rasterizerState
            {
                .FillMode{ spieler::FillMode::Solid },
                .CullMode{ spieler::CullMode::None },
                .TriangleOrder{ spieler::TriangleOrder::Clockwise }
            };

            const spieler::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ spieler::DepthState::TestState::Enabled },
                    .WriteState{ spieler::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ spieler::ComparisonFunction::LessEqual }
                },
                .StencilState
                {
                    .TestState{ spieler::StencilState::TestState::Disabled },
                }
            };

            const spieler::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ spieler::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["environment"] = spieler::GraphicsPipelineState{ m_Device, config };
        }

        // Fullscreen
        {
            const spieler::Shader* vertexShader{ nullptr };
            const spieler::Shader* pixelShader{ nullptr };

            // Init Shaders
            {
                const std::wstring_view filepath{ L"assets/shaders/fullscreen.hlsl" };

                const spieler::Shader::Config vertexShaderConfig
                {
                    .Type{ spieler::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const spieler::Shader::Config pixelShaderConfig
                {
                    .Type{ spieler::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["fullscreen_vs"] = spieler::Shader::Create(vertexShaderConfig);
                m_ShaderLibrary["fullscreen_ps"] = spieler::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["fullscreen_vs"].get();
                pixelShader = m_ShaderLibrary["fullscreen_ps"].get();
            }

            const spieler::RasterizerState rasterizerState
            {
                .FillMode{ spieler::FillMode::Solid },
                .CullMode{ spieler::CullMode::None },
            };

            const spieler::InputLayout nullInputLayout;

            const spieler::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &rasterizerState },
                .InputLayout{ &nullInputLayout },
                .PrimitiveTopologyType{ spieler::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBufferFormat() },
            };

            m_PipelineStates["fullscreen"] = spieler::GraphicsPipelineState{ m_Device, config };
        }
    }

    void InstancingAndCullingLayer::InitRenderTarget()
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::_2D },
            .Width{ m_Window.GetWidth() },
            .Height{ m_Window.GetHeight() },
            .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .UsageFlags{ spieler::TextureResource::UsageFlags::RenderTarget }
        };

        const spieler::TextureResource::ClearColor clearColor
        {
            .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
        };

        m_Textures["render_target"].SetTextureResource(spieler::TextureResource::Create(m_Device, config, clearColor));
        m_Textures["render_target"].PushView<spieler::TextureShaderResourceView>(m_Device);
        m_Textures["render_target"].PushView<spieler::TextureRenderTargetView>(m_Device);
    }

    void InstancingAndCullingLayer::InitDepthStencil()
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::_2D },
            .Width{ m_Window.GetWidth() },
            .Height{ m_Window.GetHeight() },
            .Format{ ms_DepthStencilFormat },
            .UsageFlags{ spieler::TextureResource::UsageFlags::DepthStencil }
        };

        const spieler::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        auto& depthStencil{ m_Textures["depth_stencil"] };
        depthStencil.SetTextureResource(spieler::TextureResource::Create(m_Device, config, clearDepthStencil));
        depthStencil.PushView<spieler::TextureDepthStencilView>(m_Device);
    }

    void InstancingAndCullingLayer::OnWindowResized()
    {
        InitRenderTarget();
        InitDepthStencil();

        m_BlurPass->OnResize(m_Device, m_Window.GetWidth(), m_Window.GetHeight());
    }

    void InstancingAndCullingLayer::RenderEntities(const spieler::PipelineState& pso, const std::span<const spieler::Entity*>& entities)
    {
        m_GraphicsCommandList.SetPipelineState(pso);

        m_GraphicsCommandList.SetGraphicsRawShaderResource(2, *m_Buffers.at("material_structured_buffer").GetBufferResource());
        m_GraphicsCommandList.SetGraphicsDescriptorTable(4, m_Textures.at("red").GetView<spieler::TextureShaderResourceView>());

        for (const auto& entity : entities)
        {
            const auto& meshComponent = entity->GetComponent<spieler::MeshComponent>();

            if (!meshComponent.Mesh || !meshComponent.SubMesh)
            {
                continue;
            }

            const auto& instancesComponent = entity->GetComponent<spieler::InstancesComponent>();

            m_GraphicsCommandList.SetGraphicsRawShaderResource(
                1, 
                *m_Buffers.at("render_item_structured_buffer").GetBufferResource(),
                instancesComponent.StructuredBufferOffset
            );

            m_GraphicsCommandList.IASetVertexBuffer(meshComponent.Mesh->GetVertexBuffer().GetBufferResource().get());
            m_GraphicsCommandList.IASetIndexBuffer(meshComponent.Mesh->GetIndexBuffer().GetBufferResource().get());
            m_GraphicsCommandList.IASetPrimitiveTopology(meshComponent.PrimitiveTopology);

            m_GraphicsCommandList.DrawIndexed(
                meshComponent.SubMesh->IndexCount,
                meshComponent.SubMesh->StartIndexLocation,
                meshComponent.SubMesh->BaseVertexLocation,
                instancesComponent.VisibleInstanceCount
            );
        }
    }

    void InstancingAndCullingLayer::RenderFullscreenQuad(const spieler::TextureShaderResourceView& srv)
    {
        auto& backBuffer{ m_SwapChain.GetCurrentBuffer() };

        m_PostEffectsGraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ backBuffer.GetTextureResource().get() },
            .From{ spieler::ResourceState::Present },
            .To{ spieler::ResourceState::RenderTarget }
        });

        m_PostEffectsGraphicsCommandList.SetRenderTarget(backBuffer.GetView<spieler::TextureRenderTargetView>());
        m_PostEffectsGraphicsCommandList.ClearRenderTarget(backBuffer.GetView<spieler::TextureRenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });

        m_PostEffectsGraphicsCommandList.SetViewport(m_Window.GetViewport());
        m_PostEffectsGraphicsCommandList.SetScissorRect(m_Window.GetScissorRect());
        m_PostEffectsGraphicsCommandList.SetPipelineState(m_PipelineStates["fullscreen"]);
        m_PostEffectsGraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);
        m_PostEffectsGraphicsCommandList.IASetVertexBuffer(nullptr);
        m_PostEffectsGraphicsCommandList.IASetIndexBuffer(nullptr);
        m_PostEffectsGraphicsCommandList.IASetPrimitiveTopology(spieler::PrimitiveTopology::TriangleList);
        m_PostEffectsGraphicsCommandList.SetGraphicsDescriptorTable(4, srv);
        m_PostEffectsGraphicsCommandList.DrawVertexed(6, 0);

        m_PostEffectsGraphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ backBuffer.GetTextureResource().get() },
            .From{ spieler::ResourceState::RenderTarget },
            .To{ spieler::ResourceState::Present }
        });
    }

} // namespace sandbox
