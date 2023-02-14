#include "bootstrap.hpp"

#include "instancing_and_culling_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <benzin/system/event_dispatcher.hpp>

#include <benzin/graphics/api/render_states.hpp>
#include <benzin/graphics/api/resource_view_builder.hpp>
#include <benzin/graphics/mapped_data.hpp>
#include <benzin/graphics/resource_loader.hpp>

#include <benzin/engine/geometry_generator.hpp>

#include <benzin/utility/random.hpp>

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
            alignas(16) DirectX::XMMATRIX ShadowTransform{};
            alignas(16) DirectX::XMFLOAT3 CameraPosition{};
            alignas(16) DirectX::XMFLOAT4 AmbientLight{};
            alignas(16) std::array<benzin::Light, 16> Lights;
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

    namespace
    {

        struct SphericalVector
        {
            float Theta{ 0.0f };    // XZ
            float Phi{ 0.0f };      // Y
            float Radius{ 1.0f };
        };

        DirectX::XMVECTOR SphericalToCartesian(float theta, float phi, float radius = 1.0f)
        {
            return DirectX::XMVectorSet(
                radius * DirectX::XMScalarSin(phi) * DirectX::XMScalarSin(theta),
                radius * DirectX::XMScalarCos(phi),
                radius * DirectX::XMScalarSin(phi) * DirectX::XMScalarCos(theta),
                0.0f
            );
        }

        DirectX::XMVECTOR SphericalToCartesian(const SphericalVector& sphericalVector)
        {
            return SphericalToCartesian(sphericalVector.Theta, sphericalVector.Phi, sphericalVector.Radius);
        }

        struct PickedEntityComponent
        {
            uint32_t InstanceIndex{ 0 };
            uint32_t TriangleIndex{ 0 };
        };

        class DirectionalLightController
        {
        public:
            void SetDirectionalLight(benzin::Light* directionalLight) { m_DirectionalLight = directionalLight; }

        public:
            DirectX::XMFLOAT3 GetPositionForMesh()
            {
                DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_DirectionalLight->Direction);
                position = DirectX::XMVectorScale(position, -m_SphericalCoordinates.Radius);

                DirectX::XMFLOAT3 position3;
                DirectX::XMStoreFloat3(&position3, position);

                return position3;
            }

            void OnImGuiRender()
            {
                if (!m_DirectionalLight)
                {
                    return;
                }

                ImGui::Begin("DirectionalLightController");
                {
                    ImGui::ColorEdit3("Strength", reinterpret_cast<float*>(&m_DirectionalLight->Strength));
                    ImGui::DragFloat("Radius", &m_SphericalCoordinates.Radius, 1.0f, 10.0f);
                    ImGui::SliderAngle("Theta (XZ)", &m_SphericalCoordinates.Theta, -180.0f, 180.0f);
                    ImGui::SliderAngle("Phi (Y)", &m_SphericalCoordinates.Phi, -180.0f, 180.0f);

                    DirectX::XMStoreFloat3(
                        &m_DirectionalLight->Direction,
                        DirectX::XMVectorSubtract(
                            DirectX::XMVECTOR{},
                            SphericalToCartesian(m_SphericalCoordinates.Theta, m_SphericalCoordinates.Phi)
                        )
                    );
                }
                ImGui::End();
            }

            void AddToTheta(float degrees)
            {
                float& threta = m_SphericalCoordinates.Theta;

                threta += DirectX::XMConvertToRadians(degrees);

                if (threta < -DirectX::XM_PI)
                {
                    threta += DirectX::XM_2PI;
                }

                if (threta > DirectX::XM_PI)
                {
                    threta -= DirectX::XM_2PI;
                }
            }

            void AddToPhi(float degrees)
            {
                float& phi = m_SphericalCoordinates.Phi;

                phi += DirectX::XMConvertToRadians(degrees);

                if (phi < -DirectX::XM_PI)
                {
                    phi += DirectX::XM_2PI;
                }

                if (phi > DirectX::XM_PI)
                {
                    phi -= DirectX::XM_2PI;
                }
            }

        private:
            benzin::Light* m_DirectionalLight{ nullptr };
            SphericalVector m_SphericalCoordinates
            {
                .Theta{ DirectX::XMConvertToRadians(-132) },
                .Phi{ DirectX::XMConvertToRadians(-68) },
                .Radius{ 10.0f }
            };
        };

        DirectionalLightController g_DirectionalLightController;

    } // anonymous namespace

    //////////////////////////////////////////////////////////////////////////
    /// InstancingAndCullingLayer
    //////////////////////////////////////////////////////////////////////////
    constexpr uint32_t g_RenderTargetCubeMapWidth = 200;
    constexpr uint32_t g_RenderTargetCubeMapHeight = 200;

    InstancingAndCullingLayer::InstancingAndCullingLayer(
        benzin::Window& window, 
        benzin::Device& device, 
        benzin::CommandQueue& commandQueue, 
        benzin::SwapChain& swapChain
    )
        : m_Window{ window }
        , m_Device{ device }
        , m_CommandQueue{ commandQueue }
        , m_SwapChain{ swapChain }
        , m_RenderTargetCubeMap{ device, g_RenderTargetCubeMapWidth, g_RenderTargetCubeMapHeight }
        , m_ShadowMap{ device, 2048, 2048 }
    {}

    bool InstancingAndCullingLayer::OnAttach()
    {
        {
            InitTextures();
            InitMesh();
        }

        m_BlurPass = std::make_unique<BlurPass>(m_Device, m_Window.GetWidth(), m_Window.GetHeight());

        InitBuffers();
        InitMaterials();
        InitEntities();
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

    void InstancingAndCullingLayer::OnEvent(benzin::Event& event)
    {
        m_FlyCameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<benzin::WindowResizedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnWindowResized));
        dispatcher.Dispatch<benzin::MouseButtonPressedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnMouseButtonPressed));
    }

    void InstancingAndCullingLayer::OnUpdate(float dt)
    {
        m_FlyCameraController.OnUpdate(dt);
        g_DirectionalLightController.AddToTheta(10.0f * dt);

        auto& frameContext = m_FrameContexts[m_SwapChain.GetCurrentBackBufferIndex()];

        // Pass ConstantBuffer
        {
            benzin::MappedData bufferData{ *frameContext.PassConstantBuffer };
        
            // ShadowMap
            {
                const DirectX::XMFLOAT3 scenePosition{ 0.0f, 0.0f, 0.0f };
                const float sceneRadius = 18.0f;
                m_ShadowMap.SetSceneBounds(scenePosition, sceneRadius);

                const benzin::Camera& lightCamera = m_ShadowMap.GetCamera();

                // Copy to Constant Buffer
                const cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(lightCamera.GetViewMatrix()) },
                    .Projection{ DirectX::XMMatrixTranspose(lightCamera.GetProjectionMatrix()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(lightCamera.GetViewProjectionMatrix()) },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&lightCamera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight }
                };

                bufferData.Write(&constants, sizeof(constants), frameContext.PassConstantBuffer->GetConfig().ElementSize * 7);
            }

            // Main pass
            {
                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(m_Camera.GetViewMatrix()) },
                    .Projection{ DirectX::XMMatrixTranspose(m_Camera.GetProjectionMatrix()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(m_Camera.GetViewProjectionMatrix()) },
                    .ShadowTransform{ DirectX::XMMatrixTranspose(m_ShadowMap.GetShadowTransform()) },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight }
                };

                const uint64_t lightSize = sizeof(benzin::Light);

                for (size_t i = 0; i < m_LightSourceEntities.size(); ++i)
                {
                    const auto& lightComponent = m_LightSourceEntities[i]->GetComponent<benzin::Light>();
                    memcpy_s(constants.Lights.data() + lightSize * i, lightSize, &lightComponent, lightSize);
                }

                bufferData.Write(&constants, sizeof(constants));
            }
            
            // RenderTargetCubeMap
            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const benzin::Camera& camera = m_RenderTargetCubeMap.GetCamera(faceIndex);

                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(camera.GetViewMatrix()) },
                    .Projection{ DirectX::XMMatrixTranspose(camera.GetProjectionMatrix()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(camera.GetViewProjectionMatrix()) },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight }
                };

                const uint64_t lightSize = sizeof(benzin::Light);

                for (size_t i = 0; i < m_LightSourceEntities.size(); ++i)
                {
                    const auto& lightComponent = m_LightSourceEntities[i]->GetComponent<benzin::Light>();
                    memcpy_s(constants.Lights.data() + lightSize * i, lightSize, &lightComponent, lightSize);
                }

                bufferData.Write(&constants, sizeof(constants), frameContext.PassConstantBuffer->GetConfig().ElementSize * (faceIndex + 1));
            }
        }

        // RenderItem StructuredBuffer
        {
            benzin::MappedData bufferData{ *frameContext.EntityStructuredBuffer };

            uint32_t offset = 0;

            for (auto& [name, entity] : m_Entities)
            {
                uint32_t visibleInstanceCount = 0;

                const auto& meshComponent = entity->GetComponent<benzin::MeshComponent>();
                auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();

                if (entity->HasComponent<benzin::Light>())
                {
                    auto& instanceComponent = instancesComponent.Instances[0];
                    instanceComponent.Transform.Translation = g_DirectionalLightController.GetPositionForMesh();
                }

                for (const auto& instanceComponent : instancesComponent.Instances)
                {
                    if (!meshComponent.SubMesh || !instanceComponent.IsVisible)
                    {
                        continue;
                    }

                    const DirectX::XMMATRIX viewToLocal = DirectX::XMMatrixMultiply(m_Camera.GetInverseViewMatrix(), instanceComponent.Transform.GetInverseMatrix());
                    const DirectX::BoundingFrustum localSpaceFrustum = m_Camera.GetProjection()->GetTransformedBoundingFrustum(viewToLocal);

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
                        const auto& collisionComponent = entity->GetComponent<benzin::CollisionComponent>();

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
                            !entity->HasComponent<benzin::CollisionComponent>() ||
                            entity->GetComponent<benzin::CollisionComponent>().IsCollides(localSpaceFrustum)
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
            benzin::MappedData bufferData{ *frameContext.MaterialStructuredBuffer };

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
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        auto& frameContext = m_FrameContexts[m_SwapChain.GetCurrentBackBufferIndex()];
        auto& renderTarget = m_Textures.at("RenderTarget");
        auto& depthStencil = m_Textures.at("DepthStencil");

        {
            // ShadowMap
            {
                auto& shadowMap = m_ShadowMap.GetShadowMap();

                commandList.SetViewport(m_ShadowMap.GetViewport());
                commandList.SetScissorRect(m_ShadowMap.GetScissorRect());

                commandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());
                commandList.SetGraphicsRootSignature(*m_RootSignature);

                commandList.SetResourceBarrier(*shadowMap, benzin::Resource::State::DepthWrite);

                commandList.ClearDepthStencil(shadowMap->GetDepthStencilView());
                commandList.SetRenderTarget(nullptr, &shadowMap->GetDepthStencilView());

                commandList.SetGraphicsRawConstantBuffer(0, *frameContext.PassConstantBuffer, 7);

                RenderEntities(*m_PipelineStates.at("ShadowMap"), m_DefaultEntities);

                commandList.SetResourceBarrier(*shadowMap, benzin::Resource::State::Present);
            }

            // Render to cube map
            {
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::RenderTarget);
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::DepthWrite);

                commandList.SetViewport(m_RenderTargetCubeMap.GetViewport());
                commandList.SetScissorRect(m_RenderTargetCubeMap.GetScissorRect());

                commandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());
                commandList.SetGraphicsRootSignature(*m_RootSignature);

                for (uint32_t i = 0; i < 6; ++i)
                {
                    commandList.ClearRenderTarget(m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i));
                    commandList.ClearDepthStencil(m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView());

                    commandList.SetRenderTarget(
                        &m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i),
                        &m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView()
                    );

                    commandList.SetGraphicsRawConstantBuffer(0, *frameContext.PassConstantBuffer, i + 1);
                    commandList.SetGraphicsDescriptorTable(3, m_Textures.at("Environment")->GetShaderResourceView());
                    
                    RenderEntities(*m_PipelineStates.at("Default"), m_DefaultEntities);

                    const benzin::Entity* environmentEntity = m_Entities.at("Environment").get();
                    RenderEntities(*m_PipelineStates.at("Environment"), std::span{ &environmentEntity, 1});
                }

                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::Present);
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::Present);
            }

            commandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::RenderTarget);
            commandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::DepthWrite);

            commandList.SetViewport(m_Window.GetViewport());
            commandList.SetScissorRect(m_Window.GetScissorRect());

            commandList.SetRenderTarget(&renderTarget->GetRenderTargetView(), &depthStencil->GetDepthStencilView());

            commandList.ClearRenderTarget(renderTarget->GetRenderTargetView());
            commandList.ClearDepthStencil(depthStencil->GetDepthStencilView());

            commandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());
            commandList.SetGraphicsRootSignature(*m_RootSignature);

            commandList.SetGraphicsRawConstantBuffer(0, *frameContext.PassConstantBuffer);
            commandList.SetGraphicsDescriptorTable(3, m_RenderTargetCubeMap.GetCubeMap()->GetShaderResourceView());
            commandList.SetGraphicsDescriptorTable(4, m_ShadowMap.GetShadowMap()->GetShaderResourceView());
            
            const benzin::Entity* dynamicSphereEntity = m_Entities.at("MirrorSphere").get();
            RenderEntities(*m_PipelineStates.at("Default"), std::span{ &dynamicSphereEntity, 1 });

            commandList.SetGraphicsDescriptorTable(3, m_Textures.at("Environment")->GetShaderResourceView());
            
            RenderEntities(*m_PipelineStates.at("Default"), m_DefaultEntities);
            
            const benzin::Entity* pickedEntity = m_Entities.at("Picked").get();
            RenderEntities(*m_PipelineStates.at("Picked"), std::span{ &pickedEntity, 1 });

            RenderEntities(*m_PipelineStates.at("LightSource"), m_LightSourceEntities);

            const benzin::Entity* environmentEntity = m_Entities.at("Environment").get();
            RenderEntities(*m_PipelineStates.at("Environment"), std::span{ &environmentEntity, 1});

            commandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::Present);
            commandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::Present);
        }

        {
            commandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());

            m_BlurPass->OnExecute(commandList, *m_Textures.at("RenderTarget"), BlurPassExecuteProps{ 2.5f, 2.5f, 0 });

            RenderFullscreenQuad(m_BlurPass->GetOutput()->GetShaderResourceView());
        }
    }

    void InstancingAndCullingLayer::OnImGuiRender(float dt)
    {
        m_FlyCameraController.OnImGuiRender(dt);
        g_DirectionalLightController.OnImGuiRender();

        ImGui::Begin("Culling Data");
        {   
            ImGui::Checkbox("Is Culling Enabled", &m_IsCullingEnabled);

            for (const auto& [name, entity] : m_Entities)
            {
                const auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();
                ImGui::Text("%s visible instances: %d", name.c_str(), instancesComponent.VisibleInstanceCount);
            }
        }
        ImGui::End();

        ImGui::Begin("Picked Entity");
        {
            const auto& pickedEntity = m_Entities["Picked"];
            const auto& instancesComponent = pickedEntity->GetComponent<benzin::InstancesComponent>();

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

        ImGui::Begin("Textures");
        {
            static const auto displayTexture = [&](float widgetWidth, const std::shared_ptr<benzin::TextureResource>& texture)
            {
                BENZIN_ASSERT(texture->HasShaderResourceView());

                const uint64_t gpuHandle = texture->GetShaderResourceView().GetGPUHandle();
                const float textureAspectRatio = static_cast<float>(texture->GetConfig().Width) / static_cast<float>(texture->GetConfig().Height);

                const ImVec2 textureSize
                {
                    widgetWidth,
                    widgetWidth / textureAspectRatio
                };

                ImGui::Text("%s", texture->GetDebugName().c_str());
                ImGui::Image(reinterpret_cast<ImTextureID>(gpuHandle), textureSize);
            };

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0.0f, 0.0f });

            const float widgetWidth = ImGui::GetContentRegionAvail().x;

            {
                displayTexture(widgetWidth, m_ShadowMap.GetShadowMap());

                for (const auto& [name, texture] : m_Textures)
                {
                    if (!texture->GetConfig().IsCubeMap && texture->HasShaderResourceView())
                    {
                        displayTexture(widgetWidth, texture);
                    }
                }
            }

            ImGui::PopStyleVar();
        }
        ImGui::End();
    }

    void InstancingAndCullingLayer::InitTextures()
    {
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        const auto loadTextureFromDDSFile = [&](const std::string& name, const std::wstring& filepath)
        {
            auto& texture = m_Textures[name];
            texture = m_Device.GetResourceLoader().LoadTextureResourceFromDDSFile(filepath, commandList);

            return texture.get();
        };

        std::vector<benzin::TextureResource*> textures;
        textures.push_back(loadTextureFromDDSFile("red", L"assets/textures/red.dds"));
        textures.push_back(loadTextureFromDDSFile("green", L"assets/textures/green.dds"));
        textures.push_back(loadTextureFromDDSFile("blue", L"assets/textures/blue.dds"));
        textures.push_back(loadTextureFromDDSFile("white", L"assets/textures/white.dds"));
        textures.push_back(loadTextureFromDDSFile("Environment", L"assets/textures/environment.dds"));
        textures.push_back(loadTextureFromDDSFile("bricks2", L"assets/textures/bricks2.dds"));
        textures.push_back(loadTextureFromDDSFile("bricks2_normalmap", L"assets/textures/bricks2_normalmap.dds"));
        textures.push_back(loadTextureFromDDSFile("default_normalmap", L"assets/textures/default_normalmap.dds"));

        const std::vector<benzin::Descriptor> srvDescriptors = m_Device.GetDescriptorManager().AllocateDescriptors(
            benzin::Descriptor::Type::ShaderResourceView,
            static_cast<uint32_t>(textures.size())
        );

        const auto& resourceViewBuilder = m_Device.GetResourceViewBuilder();
        for (size_t i = 0; i < textures.size(); ++i)
        {
            resourceViewBuilder.InitShaderResourceViewForDescriptor(srvDescriptors[i], *textures[i]);
            textures[i]->PushShaderResourceView(srvDescriptors[i]);
        }
    }

    void InstancingAndCullingLayer::InitMesh()
    {
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        const benzin::BoxGeometryConfig boxConfig
        {
            .Width{ 1.0f },
            .Height{ 1.0f },
            .Depth{ 1.0f }
        };

        const benzin::SphereGeometryConfig sphereConfig
        {
            .Radius{ 1.0f },
            .SliceCount{ 16 },
            .StackCount{ 16 }
        };

        const benzin::GridGeometryConfig gridConfig
        {
            .Width{ 10.0f },
            .Depth{ 10.0f },
            .RowCount{ 2 },
            .ColumnCount{ 2 }
        };

        const std::unordered_map<std::string, benzin::MeshData> submeshes
        {
            { "Box", benzin::GeometryGenerator::GenerateBox(boxConfig) },
            { "Sphere", benzin::GeometryGenerator::GenerateSphere(sphereConfig) },
            { "Grid", benzin::GeometryGenerator::GenerateGrid(gridConfig) },
        };

        m_Mesh.SetSubMeshes(m_Device, submeshes);

        commandList.UploadToBuffer(*m_Mesh.GetVertexBuffer(), m_Mesh.GetVertices().data(), m_Mesh.GetVertices().size() * sizeof(benzin::Vertex));
        commandList.UploadToBuffer(*m_Mesh.GetIndexBuffer(), m_Mesh.GetIndices().data(), m_Mesh.GetIndices().size() * sizeof(uint32_t));
    }

    void InstancingAndCullingLayer::InitBuffers()
    {
        const benzin::BufferResource::Config passConstantBufferConfig
        {
            .ElementSize{ sizeof(cb::Pass) },
            .ElementCount{ 1 + 6 + 1 },
            .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
        };

        const benzin::BufferResource::Config entityStructuredBufferConfig
        {
            .ElementSize{ sizeof(cb::Entity) },
            .ElementCount{ 1005 },
            .Flags{ benzin::BufferResource::Flags::Dynamic }
        };

        const benzin::BufferResource::Config materialStructuredBufferConfig
        {
            .ElementSize{ sizeof(cb::Material) },
            .ElementCount{ 7 },
            .Flags{ benzin::BufferResource::Flags::Dynamic }
        };

        for (size_t i = 0; i < m_FrameContexts.size(); ++i)
        {
            const std::string indexString = std::to_string(i);

            auto& frameContext = m_FrameContexts[i];
            frameContext.PassConstantBuffer = m_Device.CreateBufferResource(passConstantBufferConfig, "PassConstantBuffer" + indexString);
            frameContext.EntityStructuredBuffer = m_Device.CreateBufferResource(entityStructuredBufferConfig, "EntityStructuredBuffer" + indexString);
            frameContext.MaterialStructuredBuffer = m_Device.CreateBufferResource(materialStructuredBufferConfig, "MaterialStructuredBuffer" + indexString);
        }
    }

    void InstancingAndCullingLayer::InitRootSignature()
    {
        benzin::RootSignature::Config config;

        // Root parameters
        {
            config.RootParameters.emplace_back(benzin::RootParameter::DescriptorConfig
            {
                .Type{ benzin::Descriptor::Type::ConstantBufferView },
                .ShaderRegister{ 0 }
            });

            config.RootParameters.emplace_back(benzin::RootParameter::DescriptorConfig
            {
                .Type{ benzin::Descriptor::Type::ShaderResourceView },
                .ShaderRegister{ 1, 1 }
            });

            config.RootParameters.emplace_back(benzin::RootParameter::DescriptorConfig
            {
                .Type{ benzin::Descriptor::Type::ShaderResourceView },
                .ShaderRegister{ 1, 2 }
            });

            config.RootParameters.emplace_back(benzin::RootParameter::SingleDescriptorTableConfig
            {
                .Range
                {
                    .Type{ benzin::Descriptor::Type::ShaderResourceView },
                    .DescriptorCount{ 1 },
                    .BaseShaderRegister{ 0 }
                }
            });

            config.RootParameters.emplace_back(benzin::RootParameter::SingleDescriptorTableConfig
            {
                .Range
                {
                    .Type{ benzin::Descriptor::Type::ShaderResourceView },
                    .DescriptorCount{ 1 },
                    .BaseShaderRegister{ 1 }
                }
            });

            config.RootParameters.emplace_back(benzin::RootParameter::SingleDescriptorTableConfig
            {
                .Range
                {
                    .Type{ benzin::Descriptor::Type::ShaderResourceView },
                    .DescriptorCount{ 10 },
                    .BaseShaderRegister{ 2 }
                }
            });
        }

        // Static samplers
        {
            config.StaticSamplers.push_back(benzin::StaticSampler::GetPointWrap({ 0 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetPointClamp({ 1 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetLinearWrap({ 2 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetLinearClamp({ 3 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetAnisotropicWrap({ 4 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetAnisotropicClamp({ 5 }));
            config.StaticSamplers.push_back(benzin::StaticSampler::GetDefaultForShadow({ 6 }));
        }

        m_RootSignature = std::make_unique<benzin::RootSignature>(m_Device, config, "Default");
    }

    void InstancingAndCullingLayer::InitPipelineState()
    {
        const auto& defaultRasterizerState = benzin::RasterizerState::GetSolidNoCulling();

        const benzin::InputLayout inputLayout
        {
            benzin::InputLayoutElement{ "Position", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "Normal", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "Tangent", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "TexCoord", benzin::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
        };

        // Default PSO
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring_view filepath = L"assets/shaders/default.hlsl";

                const benzin::Shader::Config vertexShaderConfig
                {
                    .Type{ benzin::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const benzin::Shader::Config pixelShaderConfig
                {
                    .Type{ benzin::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" },
                    .Defines
                    {
                        //{ "USE_ONLY_DIFFUSEMAP_SAMPLE" },
                        //{ "WRITE_ONLY_INTERPOLATED_NORMALS" },
                        { magic_enum::enum_name(per::InstancingAndCulling::DIRECTIONAL_LIGHT_COUNT), "1" },
                        { magic_enum::enum_name(per::InstancingAndCulling::POINT_LIGHT_COUNT), "0" },
                    }
                };

                m_ShaderLibrary["VS_Default"] = std::make_shared<benzin::Shader>(vertexShaderConfig);
                m_ShaderLibrary["PS_Default"] = std::make_shared<benzin::Shader>(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["VS_Default"].get();
                pixelShader = m_ShaderLibrary["PS_Default"].get();
            }

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &defaultRasterizerState },
                .DepthState{ &benzin::DepthState::GetLess() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["Default"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Default");
        }

        // PSO for picked entity
        {
            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *m_ShaderLibrary.at("VS_Default") },
                .PixelShader{ m_ShaderLibrary.at("PS_Default").get() },
                .RasterizerState{ &defaultRasterizerState },
                .DepthState{ &benzin::DepthState::GetLessEqual() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["Picked"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Picked");
        }

        // PSO for light sources
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring_view filepath = L"assets/shaders/default.hlsl";

                const benzin::Shader::Config pixelShaderConfig
                {
                    .Type{ benzin::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" },
                    .Defines
                    {
                        { "USE_ONLY_DIFFUSEMAP_SAMPLE" }
                    }
                };

                m_ShaderLibrary["PS_LightSource"] = std::make_shared<benzin::Shader>(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["VS_Default"].get();
                pixelShader = m_ShaderLibrary["PS_LightSource"].get();
            }

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &defaultRasterizerState },
                .DepthState{ &benzin::DepthState::GetLess() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["LightSource"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "LightSource");
        }

        // PSO for Environment
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring filepath = L"assets/shaders/environment.hlsl";

                const benzin::Shader::Config vertexShaderConfig
                {
                    .Type{ benzin::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const benzin::Shader::Config pixelShaderConfig
                {
                    .Type{ benzin::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["VS_Environment"] = std::make_shared<benzin::Shader>(vertexShaderConfig);
                m_ShaderLibrary["PS_Environment"] = std::make_shared<benzin::Shader>(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["VS_Environment"].get();
                pixelShader = m_ShaderLibrary["PS_Environment"].get();
            }

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &defaultRasterizerState },
                .DepthState{ &benzin::DepthState::GetLessEqual() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["Environment"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Environment");
        }

        // Fullscreen
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring_view filepath = L"assets/shaders/fullscreen.hlsl";

                const benzin::Shader::Config vertexShaderConfig
                {
                    .Type{ benzin::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const benzin::Shader::Config pixelShaderConfig
                {
                    .Type{ benzin::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["VS_Fullscreen"] = std::make_shared<benzin::Shader>(vertexShaderConfig);
                m_ShaderLibrary["PS_Fullscreen"] = std::make_shared<benzin::Shader>(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["VS_Fullscreen"].get();
                pixelShader = m_ShaderLibrary["PS_Fullscreen"].get();
            }

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &defaultRasterizerState },
                .DepthState{ &benzin::DepthState::GetDisabled() },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
            };

            m_PipelineStates["Fullscreen"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Fullscreen");
        }

        // ShadowMap
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring_view filepath = L"assets/shaders/shadow_mapping.hlsl";

                const benzin::Shader::Config vertexShaderConfig
                {
                    .Type{ benzin::Shader::Type::Vertex },
                    .Filepath{ filepath },
                    .EntryPoint{ "VS_Main" }
                };

                const benzin::Shader::Config pixelShaderConfig
                {
                    .Type{ benzin::Shader::Type::Pixel },
                    .Filepath{ filepath },
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["VS_ShadowMap"] = std::make_shared<benzin::Shader>(vertexShaderConfig);
                m_ShaderLibrary["PS_ShadowMap"] = std::make_shared<benzin::Shader>(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["VS_ShadowMap"].get();
                pixelShader = m_ShaderLibrary["PS_ShadowMap"].get();
            }

            const benzin::RasterizerState rasterizerState
            {
                .DepthBias{ 100'000 },
                .DepthBiasClamp{ 0.0f },
                .SlopeScaledDepthBias{ 1.0f }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ *m_RootSignature },
                .VertexShader{ *vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &rasterizerState },
                .DepthState{ &benzin::DepthState::GetLess() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["ShadowMap"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "ShadowMap");
        }
    }

    void InstancingAndCullingLayer::InitRenderTarget()
    {
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ m_Window.GetWidth() },
            .Height{ m_Window.GetHeight() },
            .Format{ benzin::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ benzin::TextureResource::Flags::BindAsRenderTarget }
        };

        auto& renderTarget = m_Textures["RenderTarget"];
        renderTarget = m_Device.CreateTextureResource(config, "RenderTarget");
        renderTarget->PushRenderTargetView(m_Device.GetResourceViewBuilder().CreateRenderTargetView(*renderTarget));
        renderTarget->PushShaderResourceView(m_Device.GetResourceViewBuilder().CreateShaderResourceView(*renderTarget));
    }

    void InstancingAndCullingLayer::InitDepthStencil()
    {
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ m_Window.GetWidth() },
            .Height{ m_Window.GetHeight() },
            .Format{ ms_DepthStencilFormat },
            .Flags{ benzin::TextureResource::Flags::BindAsDepthStencil }
        };

        auto& depthStencil = m_Textures["DepthStencil"];
        depthStencil = m_Device.CreateTextureResource(config, "DepthStencil");
        depthStencil->PushDepthStencilView(m_Device.GetResourceViewBuilder().CreateDepthStencilView(*depthStencil));
    }

    void InstancingAndCullingLayer::InitMaterials()
    {
        const auto getNewMaterialStructuredBufferIndex = [this]
        {
            return static_cast<uint32_t>(m_Materials.size());
        };

        // Red
        {
            m_Materials["Red"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.05f, 0.05f, 0.05f },
                .Roughness{ 0.1f },
                .DiffuseMapIndex{ 0 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // Green
        {
            m_Materials["Green"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.05f, 0.05f, 0.05f },
                .Roughness{ 0.7f },
                .DiffuseMapIndex{ 1 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // Blue
        {
            m_Materials["Blue"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.05f, 0.05f, 0.05f },
                .Roughness{ 0.1f },
                .DiffuseMapIndex{ 2 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // White
        {
            m_Materials["White"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.05f, 0.05f, 0.05f },
                .Roughness{ 0.1f },
                .DiffuseMapIndex{ 3 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // Environment
        {
            m_Materials["Environment"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.1f, 0.1f, 0.1f },
                .Roughness{ 0.1f },
                .DiffuseMapIndex{ 4 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // Mirror
        {
            m_Materials["Mirror"] = benzin::Material
            {
                .DiffuseAlbedo{ 0.0f, 0.0f, 0.0f, 1.0f },
                .FresnelR0{ 0.98f, 0.97f, 0.95f },
                .Roughness{ 0.1f },
                .DiffuseMapIndex{ 5 },
                .NormalMapIndex{ 7 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }

        // Bricks2
        {
            m_Materials["Bricks2"] = benzin::Material
            {
                .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                .FresnelR0{ 0.05f, 0.05f, 0.05f },
                .Roughness{ 0.9f },
                .DiffuseMapIndex{ 5 },
                .NormalMapIndex{ 6 },
                .StructuredBufferIndex{ getNewMaterialStructuredBufferIndex() }
            };
        }
    }
    
    void InstancingAndCullingLayer::InitEntities()
    {
        // Environment CubeMap
        {
            auto& entity = m_Entities["Environment"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("Box");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 4;
            instanceComponent.Transform = benzin::Transform
            {
                .Scale{ 100.0f, 100.0f, 100.0f },
                .Translation{ 2.0f, 0.0f, 0.0f }
            };
        }

        // Grid
        {
            auto& entity = m_Entities["Grid"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("Grid");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& collisionComponent = entity->CreateComponent<benzin::CollisionComponent>();
            collisionComponent.CreateBoundingBox(meshComponent);

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            instancesComponent.IsNeedCulling = true;

            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 0;
            instanceComponent.Transform = benzin::Transform
            {
                .Scale{ 2.5f, 2.5f, 2.5f },
                .Translation{ 0.0f, -2.5f, 0.0f }
            };

            m_DefaultEntities.push_back(entity.get());
        }

        // Dynamic sphere
        {
            auto& entity = m_Entities["MirrorSphere"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("Sphere");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& collisionComponent = entity->CreateComponent<benzin::CollisionComponent>();
            collisionComponent.CreateBoundingSphere(meshComponent);

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            instancesComponent.IsNeedCulling = true;

            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 5;
            instanceComponent.Transform = benzin::Transform
            {
                .Scale{ 2.5f, 2.5f, 2.5f },
                .Translation{ -5.0f, 0.0f, 0.0f }
            };

            m_RenderTargetCubeMap.SetPosition({ -5.0f, 0.0f, 0.0f, 1.0f });

            m_PickableEntities.push_back(entity.get());
        }

        // Boxes
        {
            const int32_t boxInRowCount = 2;
            const int32_t boxInColumnCount = 2;
            const int32_t boxInDepthCount = 2;

            auto& entity = m_Entities["Box"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("Box");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& collisionComponent = entity->CreateComponent<benzin::CollisionComponent>();
            collisionComponent.CreateBoundingBox(meshComponent);

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
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
                        instanceComponent.Transform.Translation = DirectX::XMFLOAT3
                        {
                            static_cast<float>((i - boxInRowCount / 2) * boxInRowCount),
                            static_cast<float>((j - boxInColumnCount / 2) * boxInColumnCount),
                            static_cast<float>((k - boxInDepthCount / 2) * boxInDepthCount)
                        };
                        instanceComponent.Transform.Scale = DirectX::XMFLOAT3{ 1.0f, 1.0f, 1.0f };
                        instanceComponent.MaterialIndex = 6;
                    }
                }
            }

            m_DefaultEntities.push_back(entity.get());
            m_PickableEntities.push_back(entity.get());
        }

        // Picked
        {
            auto& entity = m_Entities["Picked"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            instancesComponent.IsNeedCulling = true;

            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 0;
            instanceComponent.IsVisible = false;

            entity->CreateComponent<PickedEntityComponent>();
        }

        // Directional light source
        {
            auto& entity = m_Entities["DirectionalLightSource"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("Sphere");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& collisionComponent = entity->CreateComponent<benzin::CollisionComponent>();
            collisionComponent.CreateBoundingSphere(meshComponent);

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            instancesComponent.IsNeedCulling = true;

            const DirectX::XMVECTOR direction{ -0.4f, -0.6f, -0.3f, 0.0f };
            const DirectX::XMVECTOR position = DirectX::XMVectorScale(direction, 2.0f);

            DirectX::XMFLOAT3 direction3;
            DirectX::XMStoreFloat3(&direction3, direction);

            DirectX::XMFLOAT3 position3;
            DirectX::XMStoreFloat3(&position3, position);

            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 3;
            instanceComponent.Transform.Translation = position3;

            auto& lightComponent = entity->CreateComponent<benzin::Light>();
            lightComponent.Direction = direction3;
            lightComponent.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };

            m_ShadowMap.SetDirectionalLight(&lightComponent);

            m_LightSourceEntities.push_back(entity.get());
            m_PickableEntities.push_back(entity.get());

            m_DirectionalLight = &lightComponent;
            g_DirectionalLightController.SetDirectionalLight(&lightComponent);
        }
    }

    void InstancingAndCullingLayer::OnWindowResized()
    {
        InitRenderTarget();
        InitDepthStencil();
    }

    void InstancingAndCullingLayer::RenderEntities(const benzin::PipelineState& pso, const std::span<const benzin::Entity*>& entities)
    {
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        auto& frameContext = m_FrameContexts[m_SwapChain.GetCurrentBackBufferIndex()];

        commandList.SetPipelineState(pso);

        commandList.SetGraphicsRawShaderResource(2, *frameContext.MaterialStructuredBuffer);
        commandList.SetGraphicsDescriptorTable(5, m_Textures.at("red")->GetShaderResourceView());

        for (const auto& entity : entities)
        {
            const auto& meshComponent = entity->GetComponent<benzin::MeshComponent>();

            if (!meshComponent.Mesh || !meshComponent.SubMesh)
            {
                continue;
            }

            const auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();

            commandList.SetGraphicsRawShaderResource(
                1, 
                *frameContext.EntityStructuredBuffer,
                instancesComponent.StructuredBufferOffset
            );

            commandList.IASetVertexBuffer(meshComponent.Mesh->GetVertexBuffer().get());
            commandList.IASetIndexBuffer(meshComponent.Mesh->GetIndexBuffer().get());
            commandList.IASetPrimitiveTopology(meshComponent.PrimitiveTopology);

            commandList.DrawIndexed(
                meshComponent.SubMesh->IndexCount,
                meshComponent.SubMesh->StartIndexLocation,
                meshComponent.SubMesh->BaseVertexLocation,
                instancesComponent.VisibleInstanceCount
            );
        }
    }

    void InstancingAndCullingLayer::RenderFullscreenQuad(const benzin::Descriptor& srv)
    {
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();
        auto& backBuffer = m_SwapChain.GetCurrentBackBuffer();

        commandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::RenderTarget);

        commandList.SetRenderTarget(&backBuffer->GetRenderTargetView());
        commandList.ClearRenderTarget(backBuffer->GetRenderTargetView());

        commandList.SetViewport(m_Window.GetViewport());
        commandList.SetScissorRect(m_Window.GetScissorRect());
        commandList.SetPipelineState(*m_PipelineStates.at("Fullscreen"));
        commandList.SetGraphicsRootSignature(*m_RootSignature);
        commandList.IASetVertexBuffer(nullptr);
        commandList.IASetIndexBuffer(nullptr);
        commandList.IASetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.SetGraphicsDescriptorTable(5, srv);
        commandList.DrawVertexed(6, 0);

        commandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::Present);
    }

    bool InstancingAndCullingLayer::OnWindowResized(benzin::WindowResizedEvent& event)
    {
        OnWindowResized();
        m_BlurPass->OnResize(m_Device, event.GetWidth(), event.GetHeight());

        return false;
    }

    bool InstancingAndCullingLayer::OnMouseButtonPressed(benzin::MouseButtonPressedEvent& event)
    {
        if (event.GetButton() == benzin::MouseButton::Right)
        {
            const PickingTechnique::Result result = m_PickingTechnique->PickTriangle(event.GetX<float>(), event.GetY<float>());
            const benzin::Entity* resultEntity = result.PickedEntity;

            auto& pickedEntity = m_Entities["Picked"];
            auto& instanceComponent = pickedEntity->GetComponent<benzin::InstancesComponent>().Instances[0];

            if (!result.PickedEntity)
            {
                instanceComponent.IsVisible = false;
            }
            else
            {
                auto& meshComponent = pickedEntity->GetComponent<benzin::MeshComponent>();
                meshComponent.Mesh = result.PickedEntity->GetComponent<benzin::MeshComponent>().Mesh;
                meshComponent.SubMesh = new benzin::SubMesh
                {
                    .VertexCount{ 3 },
                    .IndexCount{ 3 },
                    .BaseVertexLocation{ resultEntity->GetComponent<benzin::MeshComponent>().SubMesh->BaseVertexLocation },
                    .StartIndexLocation{ resultEntity->GetComponent<benzin::MeshComponent>().SubMesh->StartIndexLocation + result.TriangleIndex * 3 }
                };

                auto& collisionComponent = pickedEntity->GetOrCreateComponent<benzin::CollisionComponent>();
                collisionComponent.CreateBoundingBox(meshComponent);

                instanceComponent.IsVisible = true;
                instanceComponent.Transform = resultEntity->GetComponent<benzin::InstancesComponent>().Instances[result.InstanceIndex].Transform;

                auto& pickedEntityComponent = pickedEntity->GetComponent<PickedEntityComponent>();
                pickedEntityComponent.InstanceIndex = result.InstanceIndex;
                pickedEntityComponent.TriangleIndex = result.TriangleIndex;
            }
        }

        return false;
    }

} // namespace sandbox
