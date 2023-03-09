#include "bootstrap.hpp"

#include "instancing_and_culling_layer.hpp"

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

    namespace shader
    {

        struct Pass
        {
            DirectX::XMMATRIX View{};
            DirectX::XMMATRIX Projection{};
            DirectX::XMMATRIX ViewProjection{};
            DirectX::XMMATRIX ShadowTransform{};
            DirectX::XMFLOAT3 CameraPosition{};
            uint32_t Padding;
            DirectX::XMFLOAT4 AmbientLight{};
            benzin::Light SunLight;
        };

        struct Entity
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
            uint32_t MaterialIndex{ 0 };
            uint32_t Padding[3];
        };

        struct Material
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
            uint32_t NormalMapIndex{ 0 };
            uint32_t Padding[2];
        };

    } // namespace shader

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
        m_BlurPassExecuteArgs = BlurPass::ExecuteArgs
        {
            .HorizontalBlurSigma{ 2.5f },
            .VerticalBlurSigma{ 2.5f },
            .BlurCount{ 1 }
        };

        m_SobelFilterPass = std::make_unique<SobelFilterPass>(m_Device, m_Window.GetWidth(), m_Window.GetHeight());

        InitBuffers();
        InitMaterials();
        InitEntities();
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
            const uint32_t elementSize = frameContext.PassBuffer->GetConfig().ElementSize;

            benzin::MappedData bufferData{ *frameContext.PassBuffer };
        
            // ShadowMap
            {
                const DirectX::XMFLOAT3 scenePosition{ 0.0f, 0.0f, 0.0f };
                const float sceneRadius = 18.0f;
                m_ShadowMap.SetSceneBounds(scenePosition, sceneRadius);

                const benzin::Camera& lightCamera = m_ShadowMap.GetCamera();

                const shader::Pass constants
                {
                    .View{ lightCamera.GetViewMatrix() },
                    .Projection{ lightCamera.GetProjectionMatrix() },
                    .ViewProjection{ lightCamera.GetViewProjectionMatrix() }
                };

                bufferData.Write(&constants, sizeof(constants), elementSize * 7);
            }

            // Main pass
            {
                const shader::Pass constants
                {
                    .View{ m_Camera.GetViewMatrix() },
                    .Projection{ m_Camera.GetProjectionMatrix() },
                    .ViewProjection{ m_Camera.GetViewProjectionMatrix() },
                    .ShadowTransform{ m_ShadowMap.GetShadowTransform() },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight },
                    .SunLight{ *m_DirectionalLight }
                };

                bufferData.Write(&constants, sizeof(constants), 0);
            }
            
            // RenderTargetCubeMap
            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const benzin::Camera& camera = m_RenderTargetCubeMap.GetCamera(faceIndex);

                const shader::Pass constants
                {
                    .View{ camera.GetViewMatrix() },
                    .Projection{ camera.GetProjectionMatrix() },
                    .ViewProjection{ camera.GetViewProjectionMatrix() },
                    .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()) },
                    .AmbientLight{ m_AmbientLight },
                    .SunLight{ *m_DirectionalLight }
                };

                bufferData.Write(&constants, sizeof(constants), elementSize * (faceIndex + 1));
            }
        }

        // RenderItem StructuredBuffer
        {
            benzin::MappedData bufferData{ *frameContext.EntityBuffer };

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
                        const shader::Entity constants
                        {
                            .World{ instanceComponent.Transform.GetMatrix() },
                            .MaterialIndex{ instanceComponent.MaterialIndex }
                        };

                        bufferData.Write(&constants, sizeof(constants), (offset + visibleInstanceCount++) * sizeof(constants));
                    }
                    else
                    {
                        const auto& collisionComponent = entity->GetComponent<benzin::CollisionComponent>();

                        if (collisionComponent.IsCollides(localSpaceFrustum))
                        {
                            const shader::Entity constants
                            {
                                .World{ instanceComponent.Transform.GetMatrix() },
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
                            const shader::Entity constants
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
            benzin::MappedData bufferData{ *frameContext.MaterialBuffer };

            for (const auto& [name, material] : m_Materials)
            {
                const shader::Material constants
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
        enum : uint32_t
        {
            RootConstant_PassBufferIndex = 0,
            RootConstant_PassBufferOffset = 1,

            RootConstant_EntityBufferIndex = 2,
            RootConstant_EntityBufferOffset = 3,

            RootConstant_MaterialBufferIndex = 4,

            RootConstant_EnvironmentMapIndex = 5,
            RootConstant_ShadowMapIndex = 6
        };

        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        auto& frameContext = m_FrameContexts[m_SwapChain.GetCurrentBackBufferIndex()];

        {
            commandList.SetRootConstant(RootConstant_PassBufferIndex, frameContext.PassBuffer->GetShaderResourceView().GetHeapIndex());
            commandList.SetRootConstant(RootConstant_EntityBufferIndex, frameContext.EntityBuffer->GetShaderResourceView().GetHeapIndex());
            commandList.SetRootConstant(RootConstant_MaterialBufferIndex, frameContext.MaterialBuffer->GetShaderResourceView().GetHeapIndex());

            // Render to ShadowMap
            {
                auto& shadowMap = m_ShadowMap.GetShadowMap();

                commandList.SetViewport(m_ShadowMap.GetViewport());
                commandList.SetScissorRect(m_ShadowMap.GetScissorRect());

                commandList.SetResourceBarrier(*shadowMap, benzin::Resource::State::DepthWrite);

                commandList.ClearDepthStencil(shadowMap->GetDepthStencilView());
                commandList.SetRenderTarget(nullptr, &shadowMap->GetDepthStencilView());

                commandList.SetRootConstant(RootConstant_PassBufferOffset, 7);

                DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("ShadowMap"), m_DefaultEntities);

                commandList.SetResourceBarrier(*shadowMap, benzin::Resource::State::Present);
            }

            // Render to CubeMap
            {
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::RenderTarget);
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::DepthWrite);

                commandList.SetViewport(m_RenderTargetCubeMap.GetViewport());
                commandList.SetScissorRect(m_RenderTargetCubeMap.GetScissorRect());

                for (uint32_t i = 0; i < 6; ++i)
                {
                    commandList.SetRenderTarget(
                        &m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i),
                        &m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView()
                    );

                    commandList.ClearRenderTarget(m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i));
                    commandList.ClearDepthStencil(m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView());

                    commandList.SetRootConstant(RootConstant_PassBufferOffset, i + 1);
                    commandList.SetRootConstant(RootConstant_EnvironmentMapIndex, m_Textures.at("Environment")->GetShaderResourceView().GetHeapIndex());

                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Default"), m_DefaultEntities);

                    const benzin::Entity* environmentEntity = m_Entities.at("Environment").get();
                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Environment"), std::span{ &environmentEntity, 1 });
                }

                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::Present);
                commandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::Present);
            }

            // Default rendering
            {
                auto& renderTarget = m_Textures.at("RenderTarget");
                auto& depthStencil = m_Textures.at("DepthStencil");

                commandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::RenderTarget);
                commandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::DepthWrite);

                commandList.SetViewport(m_Window.GetViewport());
                commandList.SetScissorRect(m_Window.GetScissorRect());

                commandList.SetRenderTarget(&renderTarget->GetRenderTargetView(), &depthStencil->GetDepthStencilView());

                commandList.ClearRenderTarget(renderTarget->GetRenderTargetView());
                commandList.ClearDepthStencil(depthStencil->GetDepthStencilView());

                commandList.SetRootConstant(RootConstant_PassBufferOffset, 0);
                commandList.SetRootConstant(RootConstant_ShadowMapIndex, m_ShadowMap.GetShadowMap()->GetShaderResourceView().GetHeapIndex());

                {
                    commandList.SetRootConstant(RootConstant_EnvironmentMapIndex, m_RenderTargetCubeMap.GetCubeMap()->GetShaderResourceView().GetHeapIndex());

                    const benzin::Entity* dynamicSphereEntity = m_Entities.at("MirrorSphere").get();
                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Default"), std::span{ &dynamicSphereEntity, 1 });
                }
                
                {
                    commandList.SetRootConstant(RootConstant_EnvironmentMapIndex, m_Textures.at("Environment")->GetShaderResourceView().GetHeapIndex());

                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Default"), m_DefaultEntities);

                    const benzin::Entity* pickedEntity = m_Entities.at("Picked").get();
                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Picked"), std::span{ &pickedEntity, 1 });

                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("LightSource"), m_LightSourceEntities);

                    const benzin::Entity* environmentEntity = m_Entities.at("Environment").get();
                    DrawEntities(RootConstant_EntityBufferOffset, *m_PipelineStates.at("Environment"), std::span{ &environmentEntity, 1 });
                }

                commandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::Present);
                commandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::Present);
            }
        }

        if (m_IsSobelFilterPassEnabled)
        {
            m_SobelFilterPass->OnExecute(commandList, *m_Textures.at("RenderTarget"));

            auto& blurPassInputTexture = m_UseSobelFilterEdgeMap ? *m_SobelFilterPass->GetEdgeMap() : *m_Textures.at("RenderTarget");
            m_BlurPass->OnExecute(commandList, blurPassInputTexture, m_BlurPassExecuteArgs);
        }
        else
        {
            m_BlurPass->OnExecute(commandList, *m_Textures.at("RenderTarget"), m_BlurPassExecuteArgs);
        }

        DrawFullscreenQuad(m_BlurPass->GetOutput()->GetShaderResourceView());
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

        ImGui::Begin("PostFX");
        {
            ImGui::Text("SobelFilterPass");
            ImGui::Checkbox("IsEnabled", &m_IsSobelFilterPassEnabled);

            if (!m_IsSobelFilterPassEnabled)
            {
                ImGui::BeginDisabled();
            }

            ImGui::Checkbox("DrawEdgeMap", &m_UseSobelFilterEdgeMap);

            if (!m_IsSobelFilterPassEnabled)
            {
                ImGui::EndDisabled();
            }

            ImGui::Separator();

            ImGui::Text("BlurPass");
            ImGui::SliderFloat("HorizontalBlurSigma", &m_BlurPassExecuteArgs.HorizontalBlurSigma, 0.01f, 2.5f);
            ImGui::SliderFloat("VerticalBlurSigma", &m_BlurPassExecuteArgs.VerticalBlurSigma, 0.01f, 2.5f);
            ImGui::SliderInt("BlurCount", reinterpret_cast<int*>(&m_BlurPassExecuteArgs.BlurCount), 0, 100);
        }
        ImGui::End();
    }

    void InstancingAndCullingLayer::InitTextures()
    {
        const auto loadTextureFromDDSFile = [&](const std::string& name, const std::wstring& filepath)
        {
            auto& commandList = m_CommandQueue.GetGraphicsCommandList();
            auto& resourceViewBuilder = m_Device.GetResourceViewBuilder();

            auto& texture = m_Textures[name];
            texture = m_Device.GetResourceLoader().LoadTextureResourceFromDDSFile(filepath, commandList);
            texture->PushShaderResourceView(resourceViewBuilder.CreateShaderResourceView(*texture));
        };

        loadTextureFromDDSFile("red", L"../../assets/textures/red.dds");
        loadTextureFromDDSFile("green", L"../../assets/textures/green.dds");
        loadTextureFromDDSFile("blue", L"../../assets/textures/blue.dds");
        loadTextureFromDDSFile("white", L"../../assets/textures/white.dds");
        loadTextureFromDDSFile("Environment", L"../../assets/textures/environment.dds");
        loadTextureFromDDSFile("bricks2", L"../../assets/textures/bricks2.dds");
        loadTextureFromDDSFile("bricks2_normalmap", L"../../assets/textures/bricks2_normalmap.dds");
        loadTextureFromDDSFile("default_normalmap", L"../../assets/textures/default_normalmap.dds");
    }

    void InstancingAndCullingLayer::InitMesh()
    {
        const benzin::geometry_generator::BoxConfig boxConfig
        {
            .Width{ 1.0f },
            .Height{ 1.0f },
            .Depth{ 1.0f }
        };

        const benzin::geometry_generator::SphereConfig sphereConfig
        {
            .Radius{ 1.0f },
            .SliceCount{ 20 },
            .StackCount{ 20 }
        };

        const benzin::geometry_generator::GridConfig gridConfig
        {
            .Width{ 10.0f },
            .Depth{ 10.0f },
            .RowCount{ 2 },
            .ColumnCount{ 2 }
        };

        const std::unordered_map<std::string, benzin::MeshData> submeshes
        {
            { "Box", benzin::geometry_generator::GenerateBox(boxConfig) },
            { "Sphere", benzin::geometry_generator::GenerateSphere(sphereConfig) },
            { "Grid", benzin::geometry_generator::GenerateGrid(gridConfig) },
        };

        m_Mesh.SetSubMeshes(m_Device, submeshes);

        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        commandList.UploadToBuffer(*m_Mesh.GetVertexBuffer(), m_Mesh.GetVertices().data(), m_Mesh.GetVertices().size() * sizeof(benzin::Vertex));
        commandList.UploadToBuffer(*m_Mesh.GetIndexBuffer(), m_Mesh.GetIndices().data(), m_Mesh.GetIndices().size() * sizeof(uint32_t));
    
        // TODO: Remove (GPU Validation)
        commandList.SetResourceBarrier(*m_Mesh.GetVertexBuffer(), benzin::Resource::State::VertexBuffer);
        commandList.SetResourceBarrier(*m_Mesh.GetIndexBuffer(), benzin::Resource::State::IndexBuffer);
    }

    void InstancingAndCullingLayer::InitBuffers()
    {
        const auto createBuffer = [&](const benzin::BufferResource::Config& config, std::shared_ptr<benzin::BufferResource>& buffer, std::string_view debugName)
        {
            using namespace magic_enum::bitwise_operators;
            using Flags = benzin::BufferResource::Flags;

            auto& resourceViewBuilder = m_Device.GetResourceViewBuilder();

            buffer = m_Device.CreateBufferResource(config, debugName);
            buffer->PushShaderResourceView(resourceViewBuilder.CreateShaderResourceView(*buffer));
        };

        const benzin::BufferResource::Config passBufferConfig
        {
            .ElementSize{ sizeof(shader::Pass) },
            .ElementCount{ 8 },
            .Flags{ benzin::BufferResource::Flags::Dynamic }
        };

        const benzin::BufferResource::Config entityBufferConfig
        {
            .ElementSize{ sizeof(shader::Entity) },
            .ElementCount{ 20 },
            .Flags{ benzin::BufferResource::Flags::Dynamic }
        };

        const benzin::BufferResource::Config materialBufferConfig
        {
            .ElementSize{ sizeof(shader::Material) },
            .ElementCount{ 7 },
            .Flags{ benzin::BufferResource::Flags::Dynamic }
        };

        for (size_t i = 0; i < m_FrameContexts.size(); ++i)
        {
            const std::string indexString = std::to_string(i);

            auto& frameContext = m_FrameContexts[i];

            createBuffer(passBufferConfig, frameContext.PassBuffer, "Pass" + indexString);
            createBuffer(entityBufferConfig, frameContext.EntityBuffer, "Entity" + indexString);
            createBuffer(materialBufferConfig, frameContext.MaterialBuffer, "Material" + indexString);
        }
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
            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "default.hlsl", "VS_Main" },
                .PixelShader{ "default.hlsl", "PS_Main", { "DIRECTIONAL_LIGHT_COUNT" } },
                .RasterizerState{ defaultRasterizerState },
                .DepthState{ benzin::DepthState::GetLess() },
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
                .VertexShader{ "default.hlsl", "VS_Main" },
                .PixelShader{ "default.hlsl", "PS_Main" },
                .RasterizerState{ defaultRasterizerState },
                .DepthState{ benzin::DepthState::GetLessEqual() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["Picked"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Picked");
        }

        // PSO for light sources
        {
            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "default.hlsl", "VS_Main" },
                .PixelShader{ "default.hlsl", "PS_Main", { "USE_ONLY_DIFFUSEMAP_SAMPLE" } },
                .RasterizerState{ defaultRasterizerState },
                .DepthState{ benzin::DepthState::GetLess() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["LightSource"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "LightSource");
        }

        // PSO for Environment
        {
            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "environment.hlsl", "VS_Main" },
                .PixelShader{ "environment.hlsl", "PS_Main" },
                .RasterizerState{ defaultRasterizerState },
                .DepthState{ benzin::DepthState::GetLessEqual() },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
                .DepthStencilViewFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["Environment"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Environment");
        }

        // Fullscreen
        {
            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "full_screen_quad.hlsl", "VS_Main" },
                .PixelShader{ "full_screen_quad.hlsl", "PS_Main" },
                .RasterizerState{ defaultRasterizerState },
                .DepthState{ benzin::DepthState::GetDisabled() },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() },
            };

            m_PipelineStates["Fullscreen"] = std::make_unique<benzin::GraphicsPipelineState>(m_Device, config, "Fullscreen");
        }

        // ShadowMap
        {
            const benzin::RasterizerState rasterizerState
            {
                .DepthBias{ 100'000 },
                .DepthBiasClamp{ 0.0f },
                .SlopeScaledDepthBias{ 1.0f }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "shadow_mapping.hlsl", "VS_Main" },
                .PixelShader{ "shadow_mapping.hlsl", "PS_Main" },
                .RasterizerState{ rasterizerState },
                .DepthState{ benzin::DepthState::GetLess() },
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
        renderTarget->PushShaderResourceView(m_Device.GetResourceViewBuilder().CreateShaderResourceView(*renderTarget));
        renderTarget->PushRenderTargetView(m_Device.GetResourceViewBuilder().CreateRenderTargetView(*renderTarget));
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
        const uint32_t defaultNormalMapHeapIndex = m_Textures.at("default_normalmap")->GetShaderResourceView().GetHeapIndex();

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
                .DiffuseMapIndex{ m_Textures.at("red")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("green")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("blue")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("white")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("Environment")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("white")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ defaultNormalMapHeapIndex },
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
                .DiffuseMapIndex{ m_Textures.at("bricks2")->GetShaderResourceView().GetHeapIndex() },
                .NormalMapIndex{ m_Textures.at("bricks2_normalmap")->GetShaderResourceView().GetHeapIndex() },
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

    void InstancingAndCullingLayer::DrawEntities(uint32_t entityBufferOffsetIndex, const benzin::PipelineState& pso, const std::span<const benzin::Entity*>& entities)
    {
        auto& commandList = m_CommandQueue.GetGraphicsCommandList();

        auto& frameContext = m_FrameContexts[m_SwapChain.GetCurrentBackBufferIndex()];

        commandList.SetPipelineState(pso);

        for (const auto& entity : entities)
        {
            const auto& meshComponent = entity->GetComponent<benzin::MeshComponent>();

            if (!meshComponent.Mesh || !meshComponent.SubMesh)
            {
                continue;
            }

            commandList.IASetVertexBuffer(meshComponent.Mesh->GetVertexBuffer().get());
            commandList.IASetIndexBuffer(meshComponent.Mesh->GetIndexBuffer().get());
            commandList.IASetPrimitiveTopology(meshComponent.PrimitiveTopology);

            const auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();
            commandList.SetRootConstant(entityBufferOffsetIndex, instancesComponent.StructuredBufferOffset);

            commandList.DrawIndexed(
                meshComponent.SubMesh->IndexCount,
                meshComponent.SubMesh->StartIndexLocation,
                meshComponent.SubMesh->BaseVertexLocation,
                instancesComponent.VisibleInstanceCount
            );
        }
    }

    void InstancingAndCullingLayer::DrawFullscreenQuad(const benzin::Descriptor& srv)
    {
        enum : uint32_t
        {
            RootConstant_FullScreenTextureMapIndex
        };

        auto& commandList = m_CommandQueue.GetGraphicsCommandList();
        auto& backBuffer = m_SwapChain.GetCurrentBackBuffer();

        commandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::RenderTarget);

        commandList.SetRenderTarget(&backBuffer->GetRenderTargetView());
        commandList.ClearRenderTarget(backBuffer->GetRenderTargetView());

        commandList.SetViewport(m_Window.GetViewport());
        commandList.SetScissorRect(m_Window.GetScissorRect());
        commandList.SetPipelineState(*m_PipelineStates.at("Fullscreen"));
        commandList.IASetVertexBuffer(nullptr);
        commandList.IASetIndexBuffer(nullptr);
        commandList.IASetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        commandList.SetRootConstant(RootConstant_FullScreenTextureMapIndex, srv.GetHeapIndex());
        commandList.DrawVertexed(6, 0);

        commandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::Present);
    }

    bool InstancingAndCullingLayer::OnWindowResized(benzin::WindowResizedEvent& event)
    {
        OnWindowResized();
        m_SobelFilterPass->OnResize(m_Device, event.GetWidth(), event.GetHeight());
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
