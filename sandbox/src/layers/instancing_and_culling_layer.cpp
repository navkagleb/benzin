#include "bootstrap.hpp"

#include "instancing_and_culling_layer.hpp"

#include <third_party/imgui/imgui.h>
#include <third_party/magic_enum/magic_enum.hpp>

#include <benzin/system/event_dispatcher.hpp>

#include <benzin/graphics/depth_stencil_state.hpp>
#include <benzin/graphics/mapped_data.hpp>
#include <benzin/graphics/rasterizer_state.hpp>
#include <benzin/graphics/resource_view_builder.hpp>
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

    //////////////////////////////////////////////////////////////////////////
    //// PointLightController
    //////////////////////////////////////////////////////////////////////////
    InstancingAndCullingLayer::PointLightController::PointLightController(benzin::Light* light)
        : m_Light{ light }
    {}

    void InstancingAndCullingLayer::PointLightController::OnImGuiRender()
    {
        BENZIN_ASSERT(m_Light);

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
        , m_GraphicsCommandList{ device, "Default" }
        , m_PostEffectsGraphicsCommandList{ device, "PostEffects" }
        , m_RenderTargetCubeMap{ device, g_RenderTargetCubeMapWidth, g_RenderTargetCubeMapHeight }
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

        // Pass ConstantBuffer
        {
            benzin::MappedData bufferData{ *m_Buffers["PassConstantBuffer"] };
            
            // Main pass
            {
                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(m_Camera.GetViewMatrix()) },
                    .Projection{ DirectX::XMMatrixTranspose(m_Camera.GetProjection()->GetMatrix()) },
                    .ViewProjection{ DirectX::XMMatrixTranspose(m_Camera.GetViewProjectionMatrix()) },
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
            

            for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
            {
                const benzin::Camera& camera = m_RenderTargetCubeMap.GetCamera(faceIndex);

                cb::Pass constants
                {
                    .View{ DirectX::XMMatrixTranspose(camera.GetViewMatrix()) },
                    .Projection{ DirectX::XMMatrixTranspose(camera.GetProjection()->GetMatrix()) },
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

                bufferData.Write(&constants, sizeof(constants), m_Buffers["PassConstantBuffer"]->GetConfig().ElementSize * (faceIndex + 1));
            }
        }

        // RenderItem StructuredBuffer
        {
            benzin::MappedData bufferData{ *m_Buffers["EntityStructuredBuffer"] };

            uint32_t offset = 0;

            for (auto& [name, entity] : m_Entities)
            {
                uint32_t visibleInstanceCount = 0;

                const auto& meshComponent = entity->GetComponent<benzin::MeshComponent>();
                auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();

                if (entity->HasComponent<benzin::Light>())
                {
                    const auto& lightComponent = entity->GetComponent<benzin::Light>();
                    auto& instanceComponent = instancesComponent.Instances[0];
                    
                    DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&lightComponent.Direction);
                    position = DirectX::XMVectorScale(position, -100.0f);

                    DirectX::XMStoreFloat3(&instanceComponent.Transform.Translation, position);
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
            benzin::MappedData bufferData{ *m_Buffers["MaterialStructuredBuffer"] };

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
        auto& renderTarget = m_Textures["render_target"];
        auto& depthStencil = m_Textures["depth_stencil"];

        {
            m_GraphicsCommandList.Reset();

            // Render to cube map
            {
                m_GraphicsCommandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::RenderTarget);
                m_GraphicsCommandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::DepthWrite);

                m_GraphicsCommandList.SetViewport(m_RenderTargetCubeMap.GetViewport());
                m_GraphicsCommandList.SetScissorRect(m_RenderTargetCubeMap.GetScissorRect());

                m_GraphicsCommandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());
                m_GraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);

                for (uint32_t i = 0; i < 6; ++i)
                {
                    m_GraphicsCommandList.ClearRenderTarget(m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i), { 0.1f, 0.1f, 0.1f, 1.0f });
                    m_GraphicsCommandList.ClearDepthStencil(m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView(), 1.0f, 0);

                    m_GraphicsCommandList.SetRenderTarget(
                        m_RenderTargetCubeMap.GetCubeMap()->GetRenderTargetView(i),
                        m_RenderTargetCubeMap.GetDepthStencil()->GetDepthStencilView()
                    );

                    m_GraphicsCommandList.SetGraphicsRawConstantBuffer(0, *m_Buffers.at("PassConstantBuffer"), i + 1);
                    m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_Textures.at("environment")->GetShaderResourceView());
                    
                    RenderEntities(m_PipelineStates.at("default"), m_DefaultEntities);

                    const benzin::Entity* environmentEntity = m_Entities.at("environment").get();
                    RenderEntities(m_PipelineStates.at("environment"), std::span{ &environmentEntity, 1});
                }

                m_GraphicsCommandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetCubeMap(), benzin::Resource::State::Present);
                m_GraphicsCommandList.SetResourceBarrier(*m_RenderTargetCubeMap.GetDepthStencil(), benzin::Resource::State::Present);
            }

            m_GraphicsCommandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::RenderTarget);
            m_GraphicsCommandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::DepthWrite);

            m_GraphicsCommandList.SetViewport(m_Window.GetViewport());
            m_GraphicsCommandList.SetScissorRect(m_Window.GetScissorRect());

            m_GraphicsCommandList.SetRenderTarget(renderTarget->GetRenderTargetView(), depthStencil->GetDepthStencilView());

            m_GraphicsCommandList.ClearRenderTarget(renderTarget->GetRenderTargetView(), { 0.1f, 0.1f, 0.1f, 1.0f });
            m_GraphicsCommandList.ClearDepthStencil(depthStencil->GetDepthStencilView(), 1.0f, 0);

            m_GraphicsCommandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());
            m_GraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);

            m_GraphicsCommandList.SetGraphicsRawConstantBuffer(0, *m_Buffers.at("PassConstantBuffer"));
            m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_RenderTargetCubeMap.GetCubeMap()->GetShaderResourceView());
            
            const benzin::Entity* dynamicSphereEntity = m_Entities.at("mirror_sphere").get();
            RenderEntities(m_PipelineStates.at("default"), std::span{ &dynamicSphereEntity, 1 });

            m_GraphicsCommandList.SetGraphicsDescriptorTable(3, m_Textures.at("environment")->GetShaderResourceView());
            
            RenderEntities(m_PipelineStates.at("default"), m_DefaultEntities);
            
            const benzin::Entity* pickedEntity = m_Entities.at("picked").get();
            RenderEntities(m_PipelineStates.at("picked"), std::span{ &pickedEntity, 1 });

            RenderEntities(m_PipelineStates.at("light_source"), m_LightSourceEntities);

            const benzin::Entity* environmentEntity = m_Entities.at("environment").get();
            RenderEntities(m_PipelineStates.at("environment"), std::span{ &environmentEntity, 1});

            m_GraphicsCommandList.SetResourceBarrier(*renderTarget, benzin::Resource::State::Present);
            m_GraphicsCommandList.SetResourceBarrier(*depthStencil, benzin::Resource::State::Present);

            m_GraphicsCommandList.Close();
            m_CommandQueue.Submit(m_GraphicsCommandList);
        }

        {
            m_PostEffectsGraphicsCommandList.Reset();

            m_PostEffectsGraphicsCommandList.SetDescriptorHeaps(m_Device.GetDescriptorManager());

            m_BlurPass->OnExecute(m_PostEffectsGraphicsCommandList, *m_Textures["render_target"], BlurPassExecuteProps{ 2.5f, 2.5f, 0 });

            RenderFullscreenQuad(m_BlurPass->GetOutput()->GetShaderResourceView());

            m_PostEffectsGraphicsCommandList.Close();
            m_CommandQueue.Submit(m_PostEffectsGraphicsCommandList);
        }
    }

    void InstancingAndCullingLayer::OnImGuiRender(float dt)
    {
        m_FlyCameraController.OnImGuiRender(dt);
        m_PointLightController.OnImGuiRender();

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
            const auto& pickedEntity = m_Entities["picked"];
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

        ImGui::Begin("DynamicCube RenderItem");
        {
            auto& dynamicSphereEntity = m_Entities["mirror_sphere"];
            auto& instanceComponent = dynamicSphereEntity->GetComponent<benzin::InstancesComponent>().Instances[0];

            ImGui::DragFloat3("Position", reinterpret_cast<float*>(&instanceComponent.Transform.Translation), 0.1f);
            
            m_RenderTargetCubeMap.SetPosition(DirectX::XMLoadFloat3(&instanceComponent.Transform.Translation));
        }
        ImGui::End();

        ImGui::Begin("Textures");
        {
            for (const auto& [name, texture] : m_Textures)
            {
                if (texture->HasShaderResourceView())
                {
                    const uint64_t gpuHandle = texture->GetShaderResourceView().GetGPUHandle();
                    const float width = texture->GetConfig().Width / 2.0f;
                    const float height = texture->GetConfig().Height / 2.0f;

                    ImGui::Image(reinterpret_cast<ImTextureID>(gpuHandle), ImVec2{ width, height });
                }
            }
        }
        ImGui::End();
    }

    void InstancingAndCullingLayer::InitTextures()
    {
        const auto loadTextureFromDDSFile = [&](const std::string& name, const std::wstring& filepath)
        {
            auto& texture = m_Textures[name];
            texture = m_Device.GetResourceLoader().LoadTextureResourceFromDDSFile(filepath, m_GraphicsCommandList);

            return texture.get();
        };

        std::vector<benzin::TextureResource*> textures;
        textures.push_back(loadTextureFromDDSFile("red", L"assets/textures/red.dds"));
        textures.push_back(loadTextureFromDDSFile("green", L"assets/textures/green.dds"));
        textures.push_back(loadTextureFromDDSFile("blue", L"assets/textures/blue.dds"));
        textures.push_back(loadTextureFromDDSFile("white", L"assets/textures/white.dds"));
        textures.push_back(loadTextureFromDDSFile("environment", L"assets/textures/environment.dds"));
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
        const benzin::BoxGeometryConfig boxProps
        {
            .Width{ 1.0f },
            .Height{ 1.0f },
            .Depth{ 1.0f }
        };

        const benzin::SphereGeometryConfig sphereProps
        {
            .Radius{ 1.0f },
            .SliceCount{ 16 },
            .StackCount{ 16 }
        };

        const std::unordered_map<std::string, benzin::MeshData> submeshes
        {
            { "box", benzin::GeometryGenerator::GenerateBox(boxProps) },
            { "sphere", benzin::GeometryGenerator::GenerateSphere(sphereProps) }
        };

        m_Mesh.SetSubMeshes(m_Device, submeshes);

        m_GraphicsCommandList.UploadToBuffer(*m_Mesh.GetVertexBuffer(), m_Mesh.GetVertices().data(), m_Mesh.GetVertices().size() * sizeof(benzin::Vertex));
        m_GraphicsCommandList.UploadToBuffer(*m_Mesh.GetIndexBuffer(), m_Mesh.GetIndices().data(), m_Mesh.GetIndices().size() * sizeof(uint32_t));
    }

    void InstancingAndCullingLayer::InitBuffers()
    {
        // Pass ConstantBuffer
        {
            const std::string name = "PassConstantBuffer";

            const benzin::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Pass) },
                .ElementCount{ 7 },
                .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
            };

            m_Buffers[name] = m_Device.CreateBufferResource(config, name);
        }

        // RenderItem StructuredBuffer
        {
            const std::string name = "EntityStructuredBuffer";

            const benzin::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Entity) },
                .ElementCount{ 1004 },
                .Flags{ benzin::BufferResource::Flags::Dynamic }
            };

            m_Buffers[name] = m_Device.CreateBufferResource(config, name);
        }

        // Material StructuredBuffer
        {
            const std::string name = "MaterialStructuredBuffer";

            const benzin::BufferResource::Config config
            {
                .ElementSize{ sizeof(cb::Material) },
                .ElementCount{ 7 },
                .Flags{ benzin::BufferResource::Flags::Dynamic }
            };

            m_Buffers[name] = m_Device.CreateBufferResource(config, name);
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
                    .DescriptorCount{ 10 },
                    .BaseShaderRegister{ 1 }
                }
            });
        }

        // Static samplers
        {
            config.StaticSamplers.emplace_back(benzin::TextureFilterType::Linear, benzin::TextureAddressMode::Wrap, 0);
            config.StaticSamplers.emplace_back(benzin::TextureFilterType::Anisotropic, benzin::TextureAddressMode::Wrap, 1);
        }

        m_RootSignature = benzin::RootSignature{ m_Device, config };
    }

    void InstancingAndCullingLayer::InitPipelineState()
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
                    { magic_enum::enum_name(per::InstancingAndCulling::USE_LIGHTING), "" },
                    { magic_enum::enum_name(per::InstancingAndCulling::DIRECTIONAL_LIGHT_COUNT), "1" },
                    { magic_enum::enum_name(per::InstancingAndCulling::POINT_LIGHT_COUNT), "0" },
                }
            };

            m_ShaderLibrary["default_vs"] = benzin::Shader::Create(vertexShaderConfig);
            m_ShaderLibrary["default_ps"] = benzin::Shader::Create(pixelShaderConfig);

            vertexShader = m_ShaderLibrary["default_vs"].get();
            pixelShader = m_ShaderLibrary["default_ps"].get();
        }

        const benzin::RasterizerState rasterizerState
        {
            .FillMode{ benzin::FillMode::Solid },
            .CullMode{ benzin::CullMode::None },
            .TriangleOrder{ benzin::TriangleOrder::Clockwise }
        };

        const benzin::InputLayout inputLayout
        {
            benzin::InputLayoutElement{ "Position", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "Normal", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "Tangent", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
            benzin::InputLayoutElement{ "TexCoord", benzin::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
        };

        // Default PSO
        {
            const benzin::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ benzin::DepthState::TestState::Enabled },
                    .WriteState{ benzin::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ benzin::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ benzin::StencilState::TestState::Disabled },
                }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBackBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["default"] = benzin::GraphicsPipelineState{ m_Device, config };
        }

        // PSO for picked RenderItem
        {
            const benzin::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ benzin::DepthState::TestState::Enabled },
                    .WriteState{ benzin::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ benzin::ComparisonFunction::LessEqual }
                },
                    .StencilState
                {
                    .TestState{ benzin::StencilState::TestState::Disabled },
                }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBackBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["picked"] = benzin::GraphicsPipelineState{ m_Device, config };
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
                    .EntryPoint{ "PS_Main" }
                };

                m_ShaderLibrary["light_source_ps"] = benzin::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["default_vs"].get();
                pixelShader = m_ShaderLibrary["light_source_ps"].get();
            }

            const benzin::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ benzin::DepthState::TestState::Enabled },
                    .WriteState{ benzin::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ benzin::ComparisonFunction::Less }
                },
                .StencilState
                {
                    .TestState{ benzin::StencilState::TestState::Disabled },
                }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBackBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["light_source"] = benzin::GraphicsPipelineState{ m_Device, config };
        }

        // PSO for CubeMap
        {
            const benzin::Shader* vertexShader = nullptr;
            const benzin::Shader* pixelShader = nullptr;

            // Init Shaders
            {
                const std::wstring filepath{ L"assets/shaders/environment.hlsl" };

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

                m_ShaderLibrary["environment_vs"] = benzin::Shader::Create(vertexShaderConfig);
                m_ShaderLibrary["environment_ps"] = benzin::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["environment_vs"].get();
                pixelShader = m_ShaderLibrary["environment_ps"].get();
            }

            const benzin::RasterizerState rasterizerState
            {
                .FillMode{ benzin::FillMode::Solid },
                .CullMode{ benzin::CullMode::None },
                .TriangleOrder{ benzin::TriangleOrder::Clockwise }
            };

            const benzin::DepthStencilState depthStencilState
            {
                .DepthState
                {
                    .TestState{ benzin::DepthState::TestState::Enabled },
                    .WriteState{ benzin::DepthState::WriteState::Enabled },
                    .ComparisonFunction{ benzin::ComparisonFunction::LessEqual }
                },
                .StencilState
                {
                    .TestState{ benzin::StencilState::TestState::Disabled },
                }
            };

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .BlendState{ nullptr },
                .RasterizerState{ &rasterizerState },
                .DepthStecilState{ &depthStencilState },
                .InputLayout{ &inputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBackBufferFormat() },
                .DSVFormat{ ms_DepthStencilFormat }
            };

            m_PipelineStates["environment"] = benzin::GraphicsPipelineState{ m_Device, config };
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

                m_ShaderLibrary["fullscreen_vs"] = benzin::Shader::Create(vertexShaderConfig);
                m_ShaderLibrary["fullscreen_ps"] = benzin::Shader::Create(pixelShaderConfig);

                vertexShader = m_ShaderLibrary["fullscreen_vs"].get();
                pixelShader = m_ShaderLibrary["fullscreen_ps"].get();
            }

            const benzin::RasterizerState rasterizerState
            {
                .FillMode{ benzin::FillMode::Solid },
                .CullMode{ benzin::CullMode::None },
            };

            const benzin::InputLayout nullInputLayout;

            const benzin::GraphicsPipelineState::Config config
            {
                .RootSignature{ &m_RootSignature },
                .VertexShader{ vertexShader },
                .PixelShader{ pixelShader },
                .RasterizerState{ &rasterizerState },
                .InputLayout{ &nullInputLayout },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RTVFormat{ m_SwapChain.GetBackBufferFormat() },
            };

            m_PipelineStates["fullscreen"] = benzin::GraphicsPipelineState{ m_Device, config };
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

        const benzin::TextureResource::ClearColor clearColor
        {
            .Color{ 0.1f, 0.1f, 0.1f, 1.0f }
        };

        auto& renderTarget = m_Textures["render_target"];
        renderTarget = m_Device.CreateTextureResource(config, clearColor, "render_target");
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

        const benzin::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        auto& depthStencil = m_Textures["depth_stencil"];
        depthStencil = m_Device.CreateTextureResource(config, clearDepthStencil, "depth_stencil");
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
            m_Materials["red"] = benzin::Material
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
            m_Materials["green"] = benzin::Material
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
            m_Materials["blue"] = benzin::Material
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
            m_Materials["white"] = benzin::Material
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
            m_Materials["environment"] = benzin::Material
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
            m_Materials["mirror"] = benzin::Material
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
            m_Materials["bricks2"] = benzin::Material
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
            auto& entity = m_Entities["environment"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("box");
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

        // Dynamic sphere
        {
            auto& entity = m_Entities["mirror_sphere"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("sphere");
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

            auto& entity = m_Entities["box"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("box");
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
            auto& entity = m_Entities["picked"];
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

        // Light source
        {
            auto& entity = m_Entities["light"];
            entity = std::make_unique<benzin::Entity>();

            auto& meshComponent = entity->CreateComponent<benzin::MeshComponent>();
            meshComponent.Mesh = &m_Mesh;
            meshComponent.SubMesh = &m_Mesh.GetSubMesh("sphere");
            meshComponent.PrimitiveTopology = benzin::PrimitiveTopology::TriangleList;

            auto& collisionComponent = entity->CreateComponent<benzin::CollisionComponent>();
            collisionComponent.CreateBoundingSphere(meshComponent);

            auto& instancesComponent = entity->CreateComponent<benzin::InstancesComponent>();
            instancesComponent.IsNeedCulling = true;

            const DirectX::XMFLOAT3 position{ 0.0f, 3.0f, 0.0f };

            auto& instanceComponent = instancesComponent.Instances.emplace_back();
            instanceComponent.MaterialIndex = 3;
            instanceComponent.Transform.Translation = position;

            auto& lightComponent = entity->CreateComponent<benzin::Light>();
            lightComponent.Direction = DirectX::XMFLOAT3{ -0.5f, -0.5f, -0.5f };
            lightComponent.Strength = DirectX::XMFLOAT3{ 1.0f, 1.0f, 0.9f };
            //lightComponent.FalloffStart = 20.0f;
            //lightComponent.FalloffEnd = 23.0f;
            //lightComponent.Position = position;

            m_LightSourceEntities.push_back(entity.get());
            m_PickableEntities.push_back(entity.get());

            m_PointLightController = PointLightController{ &lightComponent };
        }
    }

    void InstancingAndCullingLayer::OnWindowResized()
    {
        InitRenderTarget();
        InitDepthStencil();
    }

    void InstancingAndCullingLayer::RenderEntities(const benzin::PipelineState& pso, const std::span<const benzin::Entity*>& entities)
    {
        m_GraphicsCommandList.SetPipelineState(pso);

        m_GraphicsCommandList.SetGraphicsRawShaderResource(2, *m_Buffers.at("MaterialStructuredBuffer"));
        m_GraphicsCommandList.SetGraphicsDescriptorTable(4, m_Textures.at("red")->GetShaderResourceView());

        for (const auto& entity : entities)
        {
            const auto& meshComponent = entity->GetComponent<benzin::MeshComponent>();

            if (!meshComponent.Mesh || !meshComponent.SubMesh)
            {
                continue;
            }

            const auto& instancesComponent = entity->GetComponent<benzin::InstancesComponent>();

            m_GraphicsCommandList.SetGraphicsRawShaderResource(
                1, 
                *m_Buffers.at("EntityStructuredBuffer"),
                instancesComponent.StructuredBufferOffset
            );

            m_GraphicsCommandList.IASetVertexBuffer(meshComponent.Mesh->GetVertexBuffer().get());
            m_GraphicsCommandList.IASetIndexBuffer(meshComponent.Mesh->GetIndexBuffer().get());
            m_GraphicsCommandList.IASetPrimitiveTopology(meshComponent.PrimitiveTopology);

            m_GraphicsCommandList.DrawIndexed(
                meshComponent.SubMesh->IndexCount,
                meshComponent.SubMesh->StartIndexLocation,
                meshComponent.SubMesh->BaseVertexLocation,
                instancesComponent.VisibleInstanceCount
            );
        }
    }

    void InstancingAndCullingLayer::RenderFullscreenQuad(const benzin::Descriptor& srv)
    {
        auto& backBuffer = m_SwapChain.GetCurrentBackBuffer();

        m_PostEffectsGraphicsCommandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::RenderTarget);

        m_PostEffectsGraphicsCommandList.SetRenderTarget(backBuffer->GetRenderTargetView());
        m_PostEffectsGraphicsCommandList.ClearRenderTarget(backBuffer->GetRenderTargetView(), { 0.1f, 0.1f, 0.1f, 1.0f });

        m_PostEffectsGraphicsCommandList.SetViewport(m_Window.GetViewport());
        m_PostEffectsGraphicsCommandList.SetScissorRect(m_Window.GetScissorRect());
        m_PostEffectsGraphicsCommandList.SetPipelineState(m_PipelineStates["fullscreen"]);
        m_PostEffectsGraphicsCommandList.SetGraphicsRootSignature(m_RootSignature);
        m_PostEffectsGraphicsCommandList.IASetVertexBuffer(nullptr);
        m_PostEffectsGraphicsCommandList.IASetIndexBuffer(nullptr);
        m_PostEffectsGraphicsCommandList.IASetPrimitiveTopology(benzin::PrimitiveTopology::TriangleList);
        m_PostEffectsGraphicsCommandList.SetGraphicsDescriptorTable(4, srv);
        m_PostEffectsGraphicsCommandList.DrawVertexed(6, 0);

        m_PostEffectsGraphicsCommandList.SetResourceBarrier(*backBuffer, benzin::Resource::State::Present);
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

            auto& pickedEntity = m_Entities["picked"];
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
