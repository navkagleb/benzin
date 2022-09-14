#include "bootstrap.hpp"

#include "instancing_and_culling_layer.hpp"

#include <third_party/imgui/imgui.h>

#include <spieler/core/application.hpp>
#include <spieler/core/logger.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/rasterizer_state.hpp>
#include <spieler/renderer/depth_stencil_state.hpp>
#include <spieler/renderer/geometry_generator.hpp>

#include <spieler/utility/random.hpp>

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
            alignas(16) std::array<InstancingAndCullingLayer::LightData, 16> Lights;
        };

        struct RenderItem
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

        enum class CubeMap
        {

        };

    } // namespace per

    //////////////////////////////////////////////////////////////////////////
    //// PointLightController
    //////////////////////////////////////////////////////////////////////////
    InstancingAndCullingLayer::PointLightController::PointLightController(LightData* light)
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

    //////////////////////////////////////////////////////////////////////////
    /// InstancingAndCullingLayer
    //////////////////////////////////////////////////////////////////////////
    bool InstancingAndCullingLayer::OnAttach()
    {
        auto& application{ spieler::Application::GetInstance() };
        auto& window{ application.GetWindow() };

        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        application.GetImGuiLayer()->SetCamera(&m_Camera);

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            // Init m_Textures
            {
                const auto initTexture = [&](const std::string& name, const std::wstring& filepath)
                {
                    auto& texture{ m_Textures[name] };
                    auto subresources{ texture.Resource.LoadFromDDSFile(device, filepath) };

                    context.UploadToTexture(texture.Resource, subresources);

                    texture.Views.Clear();
                    texture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
                };

                initTexture("red", L"assets/textures/red.dds");
                initTexture("green", L"assets/textures/green.dds");
                initTexture("blue", L"assets/textures/blue.dds");
                initTexture("white", L"assets/textures/white.dds");
                initTexture("space_cubemap", L"assets/textures/space_cubemap.dds");
            }

            // Init m_MeshGeometry
            {
                const spieler::renderer::BoxGeometryProps boxProps
                {
                    .Width{ 1.0f },
                    .Height{ 1.0f },
                    .Depth{ 1.0f }
                };

                const std::unordered_map<std::string, spieler::renderer::MeshData<uint32_t>> meshes
                {
                    { "box", spieler::renderer::GeometryGenerator::GenerateBox(boxProps) }
                };

                const uint32_t vertexSize{ sizeof(spieler::renderer::Vertex) };
                const uint32_t indexSize{ sizeof(uint32_t) };

                size_t vertexOffset{ 0 };
                size_t indexOffset{ 0 };

                for (const auto& [name, mesh] : meshes)
                {
                    m_MeshGeometry.Submeshes[name] = spieler::renderer::SubmeshGeometry
                    {
                        .IndexCount{ static_cast<uint32_t>(mesh.Indices.size()) },
                        .BaseVertexLocation{ static_cast<uint32_t>(vertexOffset) },
                        .StartIndexLocation{ static_cast<uint32_t>(indexOffset) }
                    };

                    m_MeshGeometry.Vertices.resize(m_MeshGeometry.Vertices.size() + mesh.Vertices.size());
                    m_MeshGeometry.Indices.resize(m_MeshGeometry.Indices.size() + mesh.Indices.size());

                    memcpy_s(m_MeshGeometry.Vertices.data() + vertexOffset, mesh.Vertices.size() * vertexSize, mesh.Vertices.data(), mesh.Vertices.size() * vertexSize);
                    memcpy_s(m_MeshGeometry.Indices.data() + indexOffset, mesh.Indices.size() * indexSize, mesh.Indices.data(), mesh.Indices.size() * indexSize);

                    DirectX::BoundingBox::CreateFromPoints(
                        m_MeshGeometry.Submeshes[name].BoundingBox,
                        mesh.Vertices.size(),
                        reinterpret_cast<const DirectX::XMFLOAT3*>(mesh.Vertices.data()),
                        sizeof(spieler::renderer::Vertex)
                    );

                    vertexOffset += mesh.Vertices.size();
                    indexOffset += mesh.Indices.size();
                }

                // VertexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ vertexSize },
                        .ElementCount{ static_cast<uint32_t>(m_MeshGeometry.Vertices.size()) },
                    };

                    m_MeshGeometry.VertexBuffer.Resource = spieler::renderer::BufferResource{ device, config };
                    m_MeshGeometry.VertexBuffer.Views.CreateView<spieler::renderer::VertexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.VertexBuffer.Resource, m_MeshGeometry.Vertices.data(), m_MeshGeometry.Vertices.size() * vertexSize);
                }

                // IndexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ indexSize },
                        .ElementCount{ static_cast<uint32_t>(m_MeshGeometry.Indices.size()) }
                    };

                    m_MeshGeometry.IndexBuffer.Resource = spieler::renderer::BufferResource{ device, config };
                    m_MeshGeometry.IndexBuffer.Views.CreateView<spieler::renderer::IndexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.IndexBuffer.Resource, m_MeshGeometry.Indices.data(), m_MeshGeometry.Indices.size() * indexSize);
                }
            }
        }

        // Init m_Buffers
        {
            // Pass ConstantBuffer
            {
                auto& buffer{ m_Buffers["pass_constant_buffer"] };
                
                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(cb::Pass) },
                    .ElementCount{ 1 },
                    .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
                };

                buffer.Resource = spieler::renderer::BufferResource{ device, config };
            }

            // RenderItem StructuredBuffer
            {
                auto& buffer{ m_Buffers["render_item_structured_buffer"] };

                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(cb::RenderItem) },
                    .ElementCount{ 1002 },
                    .Flags{ spieler::renderer::BufferResource::Flags::Dynamic }
                };

                buffer.Resource = spieler::renderer::BufferResource{ device, config };
            }

            // Material StructuredBuffer
            {
                auto& buffer{ m_Buffers["material_structured_buffer"] };

                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(cb::Material) },
                    .ElementCount{ 5 },
                    .Flags{ spieler::renderer::BufferResource::Flags::Dynamic }
                };

                buffer.Resource = spieler::renderer::BufferResource{ device, config };
            }
        }

        // Init m_PassData
        {
            m_PassData.Lights.push_back(LightData
            {
                .Strength{ 1.0f, 1.0f, 0.9f },
                .FalloffStart{ 20.0f },
                .Direction{ 0.5f, 0.5f, 0.5f },
                .FalloffEnd{ 21.0f },
                .Position{ 0.0f, 3.0f, 0.0f }
            });

            m_PointLightController = PointLightController{ &m_PassData.Lights[0] };
        }

        // Init m_Materials
        {
            // Red
            {
                m_Materials["red"] = MaterialData
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 0 },
                    .StructuredBufferIndex{ 0 }
                };
            }

            // Green
            {
                m_Materials["green"] = MaterialData
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.9f },
                    .DiffuseMapIndex{ 1 },
                    .StructuredBufferIndex{ 1 }
                };
            }

            // Blue
            {
                m_Materials["blue"] = MaterialData
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 2 },
                    .StructuredBufferIndex{ 2 }
                };
            }

            // White
            {
                m_Materials["white"] = MaterialData
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.05f, 0.05f, 0.05f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 3 },
                    .StructuredBufferIndex{ 3 }
                };
            }

            // CubeMap
            {
                m_Materials["cubemap"] = MaterialData
                {
                    .DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f },
                    .FresnelR0{ 0.1f, 0.1f, 0.1f },
                    .Roughness{ 0.1f },
                    .DiffuseMapIndex{ 4 },
                    .StructuredBufferIndex{ 4 }
                };
            }
        }

        // Init m_RenderItems
        {
            // CubeMap
            {
                auto& renderItem{ m_RenderItems["cubemap"] = std::make_unique<RenderItem>() };

                renderItem->MeshGeometry = &m_MeshGeometry;
                renderItem->SubmeshGeometry = &m_MeshGeometry.Submeshes["box"];
                renderItem->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                renderItem->IsNeedCulling = false;

                renderItem->Instances.push_back(RenderItem::Instance
                {
                    .Transform
                    {
                        .Scale{ 100.0f, 100.0f, 100.0f }
                    },
                    .MaterialIndex{ 4 },
                    .Visible{ true }
                });

                m_CubeMapRenderItem = renderItem.get();
            }

            // Boxes
            {
                const int32_t boxInRowCount{ 10 };
                const int32_t boxInColumnCount{ 10 };
                const int32_t boxInDepthCount{ 10 };

                auto& renderItem{ m_RenderItems["box"] = std::make_unique<RenderItem>() };
                renderItem->MeshGeometry = &m_MeshGeometry;
                renderItem->SubmeshGeometry = &m_MeshGeometry.Submeshes["box"];
                renderItem->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                renderItem->Instances.resize(boxInRowCount * boxInColumnCount * boxInDepthCount);

                for (int32_t i = 0; i < boxInRowCount; ++i)
                {
                    for (int32_t j = 0; j < boxInColumnCount; ++j)
                    {
                        for (int32_t k = 0; k < boxInDepthCount; ++k)
                        {
                            const auto index{ static_cast<size_t>((i * boxInRowCount + j) * boxInColumnCount + k) };

                            renderItem->Instances[index].Transform.Translation = DirectX::XMFLOAT3{ static_cast<float>((i - 5) * 10), static_cast<float>((j - 5) * 10), static_cast<float>((k - 5) * 10) };
                            renderItem->Instances[index].Transform.Scale = DirectX::XMFLOAT3{ 3.0f, 3.0f, 3.0f };
                            renderItem->Instances[index].MaterialIndex = spieler::Random::GetInstance().GetIntegral(0, 1);
                        }
                    }
                }

                m_DefaultRenderItems.push_back(renderItem.get());
            }

            // Picked
            {
                auto& renderItem{ m_RenderItems["picked"] = std::make_unique<RenderItem>() };

                renderItem->MeshGeometry = &m_MeshGeometry;
                renderItem->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                renderItem->Instances.push_back(RenderItem::Instance
                {
                    .MaterialIndex{ 2 },
                    .Visible{ false }
                });

                m_PickedRenderItem.RenderItem = renderItem.get();
            }

            // Light
            {
                auto& renderItem{ m_RenderItems["light"] = std::make_unique<RenderItem>() };
                renderItem->MeshGeometry = &m_MeshGeometry;
                renderItem->SubmeshGeometry = &m_MeshGeometry.Submeshes["box"];
                renderItem->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                renderItem->Instances.push_back(RenderItem::Instance
                {
                    .MaterialIndex{ 3 },
                    .Visible{ true }
                });

                m_LightRenderItems.push_back(renderItem.get());
            }
        }

        // Init m_RootSignature
        {
            spieler::renderer::RootSignature::Config config{ 5, 2 };

            // Root parameters
            {
                const spieler::renderer::RootParameter::Descriptor passConstantBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                const spieler::renderer::RootParameter::Descriptor renderItemStructuredBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ShaderResourceView },
                    .ShaderRegister{ 1, 1 }
                };

                const spieler::renderer::RootParameter::Descriptor materialStructuredBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ShaderResourceView },
                    .ShaderRegister{ 1, 2 }
                };

                const spieler::renderer::RootParameter::SingleDescriptorTable cubeMap
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 1 },
                        .BaseShaderRegister{ 0 }
                    }
                };

                const spieler::renderer::RootParameter::SingleDescriptorTable textureTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 4 },
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
                config.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Linear, spieler::renderer::TextureAddressMode::Wrap, 0 };
                config.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilterType::Anisotropic, spieler::renderer::TextureAddressMode::Wrap, 1 };
            }

            m_RootSignature = spieler::renderer::RootSignature{ device, config };
        }

        // Init m_PipelineState
        {
            const auto& swapChain{ renderer.GetSwapChain() };

            const spieler::renderer::Shader* vertexShader{ nullptr };
            const spieler::renderer::Shader* pixelShader{ nullptr };

            // Init Shaders
            {
                const std::wstring filename{ L"assets/shaders/instancing_and_culling.hlsl" };

                vertexShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::InstancingAndCulling>
                {
                    .Filename{ filename },
                    .EntryPoint{ "VS_Main" },
                });

                spieler::renderer::ShaderPermutation<per::InstancingAndCulling> pixelShaderPermutation;
                pixelShaderPermutation.Set<per::InstancingAndCulling::USE_LIGHTING>(true);
                pixelShaderPermutation.Set<per::InstancingAndCulling::DIRECTIONAL_LIGHT_COUNT>(0);
                pixelShaderPermutation.Set<per::InstancingAndCulling::POINT_LIGHT_COUNT>(1);

                pixelShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::InstancingAndCulling>
                {
                    .Filename{ filename },
                    .EntryPoint{ "PS_Main" },
                    .Permutation{ pixelShaderPermutation }
                });
            }

            const spieler::renderer::RasterizerState rasterizerState
            {
                .FillMode{ spieler::renderer::FillMode::Solid },
                .CullMode{ spieler::renderer::CullMode::None },
                .TriangleOrder{ spieler::renderer::TriangleOrder::Clockwise }
            };

            const spieler::renderer::InputLayout inputLayout
            {
                spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Normal", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Tangent", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "TexCoord", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
            };

            // Default PSO
            {
                const spieler::renderer::DepthStencilState depthStencilState
                {
                    .DepthState
                    {
                        .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                        .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                        .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                    },
                    .StencilState
                    {
                        .TestState{ spieler::renderer::StencilState::TestState::Disabled },
                    }
                };

                const spieler::renderer::GraphicsPipelineState::Config config
                {
                    .RootSignature{ &m_RootSignature },
                    .VertexShader{ vertexShader },
                    .PixelShader{ pixelShader },
                    .BlendState{ nullptr },
                    .RasterizerState{ &rasterizerState },
                    .DepthStecilState{ &depthStencilState },
                    .InputLayout{ &inputLayout },
                    .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                    .RTVFormat{ swapChain.GetBufferFormat() },
                    .DSVFormat{ ms_DepthStencilFormat }
                };

                m_PipelineStates["default"] = spieler::renderer::GraphicsPipelineState{ device, config };
            }

            // PSO for picked RenderItem
            {
                const spieler::renderer::DepthStencilState depthStencilState
                {
                    .DepthState
                    {
                        .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                        .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                        .ComparisonFunction{ spieler::renderer::ComparisonFunction::LessEqual }
                    },
                        .StencilState
                    {
                        .TestState{ spieler::renderer::StencilState::TestState::Disabled },
                    }
                };

                const spieler::renderer::GraphicsPipelineState::Config config
                {
                    .RootSignature{ &m_RootSignature },
                    .VertexShader{ vertexShader },
                    .PixelShader{ pixelShader },
                    .BlendState{ nullptr },
                    .RasterizerState{ &rasterizerState },
                    .DepthStecilState{ &depthStencilState },
                    .InputLayout{ &inputLayout },
                    .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                    .RTVFormat{ swapChain.GetBufferFormat() },
                    .DSVFormat{ ms_DepthStencilFormat }
                };

                m_PipelineStates["picked"] = spieler::renderer::GraphicsPipelineState{ device, config };
            }

            // PSO for lighting
            {
                const spieler::renderer::Shader* vertexShader{ nullptr };
                const spieler::renderer::Shader* pixelShader{ nullptr };

                // Init Shaders
                {
                    const std::wstring filename{ L"assets/shaders/instancing_and_culling.hlsl" };

                    vertexShader = &m_ShaderLibrary.GetShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderPermutation<per::InstancingAndCulling>{});

                    pixelShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::InstancingAndCulling>
                    {
                        .Filename{ filename },
                        .EntryPoint{ "PS_Main" },
                    });
                }

                const spieler::renderer::DepthStencilState depthStencilState
                {
                    .DepthState
                    {
                        .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                        .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                        .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                    },
                    .StencilState
                    {
                        .TestState{ spieler::renderer::StencilState::TestState::Disabled },
                    }
                };

                const spieler::renderer::GraphicsPipelineState::Config config
                {
                    .RootSignature{ &m_RootSignature },
                    .VertexShader{ vertexShader },
                    .PixelShader{ pixelShader },
                    .BlendState{ nullptr },
                    .RasterizerState{ &rasterizerState },
                    .DepthStecilState{ &depthStencilState },
                    .InputLayout{ &inputLayout },
                    .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                    .RTVFormat{ swapChain.GetBufferFormat() },
                    .DSVFormat{ ms_DepthStencilFormat }
                };

                m_PipelineStates["lighting"] = spieler::renderer::GraphicsPipelineState{ device, config };
            }

            // PSO for CubeMap
            {
                const spieler::renderer::Shader* vertexShader{ nullptr };
                const spieler::renderer::Shader* pixelShader{ nullptr };

                // Init Shaders
                {
                    const std::wstring filename{ L"assets/shaders/cube_map.hlsl" };

                    vertexShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::CubeMap>
                    {
                        .Filename{ filename },
                        .EntryPoint{ "VS_Main" }
                    });

                    pixelShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::CubeMap>
                    {
                        .Filename{ filename },
                        .EntryPoint{ "PS_Main" }
                    });
                }

                const spieler::renderer::RasterizerState rasterizerState
                {
                    .FillMode{ spieler::renderer::FillMode::Solid },
                    .CullMode{ spieler::renderer::CullMode::None },
                    .TriangleOrder{ spieler::renderer::TriangleOrder::Clockwise }
                };

                const spieler::renderer::DepthStencilState depthStencilState
                {
                    .DepthState
                    {
                        .TestState{ spieler::renderer::DepthState::TestState::Enabled },
                        .WriteState{ spieler::renderer::DepthState::WriteState::Enabled },
                        .ComparisonFunction{ spieler::renderer::ComparisonFunction::LessEqual }
                    },
                    .StencilState
                    {
                        .TestState{ spieler::renderer::StencilState::TestState::Disabled },
                    }
                };

                const spieler::renderer::GraphicsPipelineState::Config config
                {
                    .RootSignature{ &m_RootSignature },
                    .VertexShader{ vertexShader },
                    .PixelShader{ pixelShader },
                    .BlendState{ nullptr },
                    .RasterizerState{ &rasterizerState },
                    .DepthStecilState{ &depthStencilState },
                    .InputLayout{ &inputLayout },
                    .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Triangle },
                    .RTVFormat{ swapChain.GetBufferFormat() },
                    .DSVFormat{ ms_DepthStencilFormat }
                };

                m_PipelineStates["cubemap"] = spieler::renderer::GraphicsPipelineState{ device, config };
            }
        }

        OnWindowResized();

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
                PickTriangle(event.GetX<float>(), event.GetY<float>());
            }

            return false;
        });
    }

    void InstancingAndCullingLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Pass ConstantBuffer
        {
            cb::Pass constants
            {
                .View{ DirectX::XMMatrixTranspose(m_Camera.GetView()) },
                .Projection{ DirectX::XMMatrixTranspose(m_Camera.GetProjection()) },
                .ViewProjection{ DirectX::XMMatrixTranspose(m_Camera.GetViewProjection()) },
                .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()) },
                .AmbientLight{ m_PassData.AmbientLight }
            };

            const uint64_t lightsSize{ m_PassData.Lights.size() * sizeof(LightData) };
            memcpy_s(constants.Lights.data(), lightsSize, m_PassData.Lights.data(), lightsSize);

            m_Buffers["pass_constant_buffer"].Resource.Write(0, &constants, sizeof(constants));
        }

        // RenderItem StructuredBuffer
        {
            uint32_t offset{ 0 };

            for (auto& [name, renderItem] : m_RenderItems)
            {
                uint32_t visibleInstanceCount{ 0 };

                if (name == "light")
                {
                    renderItem->Instances[0].Transform.Translation = m_PassData.Lights[0].Position;
                }

                for (size_t i = 0; i < renderItem->Instances.size(); ++i)
                {
                    if (!renderItem->Instances[i].Visible)
                    {
                        continue;
                    }

                    const DirectX::XMMATRIX viewToLocal{ DirectX::XMMatrixMultiply(m_Camera.GetInverseView(), renderItem->Instances[i].Transform.GetInverseMatrix()) };

                    DirectX::BoundingFrustum localSpaceFrustum;
                    m_Camera.GetBoundingFrustum().Transform(localSpaceFrustum, viewToLocal);

                    if (!m_IsCullingEnabled || !renderItem->IsNeedCulling || localSpaceFrustum.Contains(renderItem->SubmeshGeometry->BoundingBox) != DirectX::DISJOINT)
                    {
                        const cb::RenderItem constants
                        {
                            .World{ DirectX::XMMatrixTranspose(renderItem->Instances[i].Transform.GetMatrix()) },
                            .MaterialIndex{ renderItem->Instances[i].MaterialIndex }
                        };

                        m_Buffers["render_item_structured_buffer"].Resource.Write((offset + visibleInstanceCount++) * sizeof(constants), &constants, sizeof(constants));
                    }
                }

                renderItem->VisibleInstanceCount = visibleInstanceCount;
                renderItem->StructuredBufferOffset = offset;

                offset += visibleInstanceCount;
            }
        }

        // Material StructuredBuffer
        {
            for (const auto& [name, material] : m_Materials)
            {
                const cb::Material constants
                {
                    .DiffuseAlbedo{ material.DiffuseAlbedo },
                    .FresnelR0{ material.FresnelR0 },
                    .Roughness{ material.Roughness },
                    .Transform{ material.Transform },
                    .DiffuseMapIndex{ material.DiffuseMapIndex },
                };

                m_Buffers["material_structured_buffer"].Resource.Write(material.StructuredBufferIndex * sizeof(constants), &constants, sizeof(constants));
            }
        }
    }

    void InstancingAndCullingLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& descriptorManager{ renderer.GetDevice().GetDescriptorManager() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto* dx12CommandList{ context.GetDX12GraphicsCommandList() };

        auto& backBuffer{ swapChain.GetCurrentBuffer() };
        auto& depthStencil{ m_Textures["depth_stencil"] };

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &backBuffer.Resource },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::RenderTarget }
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &depthStencil.Resource},
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::DepthWrite }
            });

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            context.SetRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), depthStencil.Views.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(depthStencil.Views.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::renderer::DescriptorHeap::Type::SRV));

            RenderRenderItems(m_PipelineStates.at("default"), m_DefaultRenderItems);
            RenderRenderItems(m_PipelineStates.at("picked"), std::span{ &m_PickedRenderItem.RenderItem, 1 });
            RenderRenderItems(m_PipelineStates.at("lighting"), m_LightRenderItems);
            RenderRenderItems(m_PipelineStates.at("cubemap"), std::span{ &m_CubeMapRenderItem, 1 });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &backBuffer.Resource },
                .From{ spieler::renderer::ResourceState::RenderTarget },
                .To{ spieler::renderer::ResourceState::Present }
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &depthStencil.Resource },
                .From{ spieler::renderer::ResourceState::DepthWrite },
                .To{ spieler::renderer::ResourceState::Present }
            });
        }
    }

    void InstancingAndCullingLayer::OnImGuiRender(float dt)
    {
        m_PointLightController.OnImGuiRender();

        ImGui::Begin("Culling Data");
        {   
            ImGui::Checkbox("Is Culling Enabled", &m_IsCullingEnabled);

            for (const auto& [name, renderItem] : m_RenderItems)
            {
                ImGui::Text("%s visible instances: %d", name.c_str(), renderItem->VisibleInstanceCount);
            }
        }
        ImGui::End();

        ImGui::Begin("Picked RenderItem");
        {
            if (!m_PickedRenderItem.RenderItem || !m_PickedRenderItem.RenderItem->Instances[0].Visible)
            {
                ImGui::Text("Null");
            }
            else
            {
                ImGui::Text("Instance Index: %d", m_PickedRenderItem.InstanceIndex);
                ImGui::Text("Triangle Index: %d", m_PickedRenderItem.TriangleIndex);
            }
        }
        ImGui::End();
    }

    void InstancingAndCullingLayer::InitViewport()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_Viewport = spieler::renderer::Viewport
        {
            .X{ 0.0f },
            .Y{ 0.0f },
            .Width{ static_cast<float>(window.GetWidth()) },
            .Height{ static_cast<float>(window.GetHeight()) },
            .MinDepth{ 0.0f },
            .MaxDepth{ 1.0f }
        };
    }

    void InstancingAndCullingLayer::InitScissorRect()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_ScissorRect = spieler::renderer::ScissorRect
        {
            .X{ 0.0f },
            .Y{ 0.0f },
            .Width{ static_cast<float>(window.GetWidth()) },
            .Height{ static_cast<float>(window.GetHeight()) }
        };
    }

    void InstancingAndCullingLayer::InitDepthStencil()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::TextureResource::Config config
        {
            .Type{ spieler::renderer::TextureResource::Type::_2D },
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Depth{ 1 },
            .MipCount{ 1 },
            .Format{ ms_DepthStencilFormat },
            .UsageFlags{ spieler::renderer::TextureResource::UsageFlags::DepthStencil }
        };

        const spieler::renderer::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        auto& depthStencil{ m_Textures["depth_stencil"] };

        depthStencil.Resource = spieler::renderer::TextureResource{ device, config, clearDepthStencil };

        depthStencil.Views.Clear();
        depthStencil.Views.CreateView<spieler::renderer::DepthStencilView>(device);
    }

    void InstancingAndCullingLayer::OnWindowResized()
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();
    }

    void InstancingAndCullingLayer::PickTriangle(float x, float y)
    {
        const uint32_t width{ spieler::Application::GetInstance().GetWindow().GetWidth() };
        const uint32_t height{ spieler::Application::GetInstance().GetWindow().GetHeight() };

        m_PickedRenderItem.RenderItem->Instances[0].Visible = false;

        // In View Space
        const DirectX::XMVECTOR viewRayOrigin{ 0.0f, 0.0f, 0.0f, 1.0f };
        const DirectX::XMVECTOR viewRayDirection
        {
            (2.0f * x / static_cast<float>(width) - 1.0f) / DirectX::XMVectorGetByIndex(m_Camera.GetProjection().r[0], 0),
            (-2.0f * y / static_cast<float>(height) + 1.0f) / DirectX::XMVectorGetByIndex(m_Camera.GetProjection().r[1], 1),
            1.0f,
            0.0f
        };

        float minDistance{ std::numeric_limits<float>::max() };

        for (const auto& renderItem : m_DefaultRenderItems)
        {
            const spieler::renderer::Vertex* vertices
            { 
                reinterpret_cast<const spieler::renderer::Vertex*>(renderItem->MeshGeometry->Vertices.data()) + renderItem->SubmeshGeometry->BaseVertexLocation
            };

            const uint32_t* indices
            {
                reinterpret_cast<const uint32_t*>(renderItem->MeshGeometry->Indices.data()) + renderItem->SubmeshGeometry->StartIndexLocation
            };

            float minInstanceDistance{ std::numeric_limits<float>::max() };
            uint32_t minInstanceIndex{ 0 };
            uint32_t minTriangleIndex{ 0 };

            for (size_t instanceIndex = 0; instanceIndex < renderItem->Instances.size(); ++instanceIndex)
            {
                if (!renderItem->Instances[instanceIndex].Visible)
                {
                    continue;
                }

                const DirectX::XMMATRIX toLocal{ DirectX::XMMatrixMultiply(m_Camera.GetInverseView(), renderItem->Instances[instanceIndex].Transform.GetInverseMatrix()) };

                const DirectX::XMVECTOR rayOrigin{ DirectX::XMVector3TransformCoord(viewRayOrigin, toLocal) };
                const DirectX::XMVECTOR rayDirection{ DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(viewRayDirection, toLocal)) };

                float instanceDistance{ 0.0f };

                if (!renderItem->SubmeshGeometry->BoundingBox.Intersects(rayOrigin, rayDirection, instanceDistance))
                {
                    continue;
                }

                if (minInstanceDistance > instanceDistance)
                {
                    // Min Instance
                    minInstanceDistance = instanceDistance;
                    minInstanceIndex = static_cast<uint32_t>(instanceIndex);

                    float minTriangleDistance{ std::numeric_limits<float>::max() };

                    for (uint32_t triangleIndex = 0; triangleIndex < renderItem->SubmeshGeometry->IndexCount / 3; ++triangleIndex)
                    {
                        // Triangle
                        const DirectX::XMVECTOR a{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 0]].Position) };
                        const DirectX::XMVECTOR b{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 1]].Position) };
                        const DirectX::XMVECTOR c{ DirectX::XMLoadFloat3(&vertices[indices[triangleIndex * 3 + 2]].Position) };

                        float triangleDistance{ 0.0f };

                        if (DirectX::TriangleTests::Intersects(rayOrigin, rayDirection, a, b, c, triangleDistance))
                        {
                            if (minTriangleDistance > triangleDistance)
                            {
                                // Min Instance Triangle
                                minTriangleDistance = triangleDistance;
                                minTriangleIndex = triangleIndex;
                            }
                        }
                    }
                }
            }

            if (minDistance > minInstanceDistance)
            {
                // Min RenderItem
                minDistance = minInstanceDistance;

                // Picked RenderItem
                m_PickedRenderItem.RenderItem->SubmeshGeometry = new spieler::renderer::SubmeshGeometry
                {
                    .IndexCount{ 3 },
                    .BaseVertexLocation{ renderItem->SubmeshGeometry->BaseVertexLocation },
                    .StartIndexLocation{ minTriangleIndex * 3 }
                };

                m_PickedRenderItem.RenderItem->Instances[0].Visible = true;
                m_PickedRenderItem.RenderItem->Instances[0].Transform = renderItem->Instances[minInstanceIndex].Transform;

                m_PickedRenderItem.InstanceIndex = static_cast<uint32_t>(minInstanceIndex);
                m_PickedRenderItem.TriangleIndex = minTriangleIndex;
            }
        }
    }

    void InstancingAndCullingLayer::RenderRenderItems(const spieler::renderer::PipelineState& pso, const std::span<RenderItem*>& renderItems) const
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& context{ renderer.GetContext() };
        auto* dx12CommandList{ context.GetDX12GraphicsCommandList() };

        context.SetPipelineState(pso);
        context.SetGraphicsRootSignature(m_RootSignature);

#if 0
        context.SetGraphicsRawConstantBufferView(0, m_Buffers.at("pass_constant_buffer").Resource);
        context.SetGraphicsRawShaderResourceView(2, m_Buffers.at("material_structured_buffer").Resource);
        context.SetGraphicsShaderResourceView(3, m_Textures.at("space_cubemap").Views.GetView<spieler::renderer::ShaderResourceView>());
        context.SetGraphicsShaderResourceViewRange(4, m_Textures.at("red").Views.GetView<spieler::renderer::ShaderResourceView>());
#endif

        dx12CommandList->SetGraphicsRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS{ m_Buffers.at("pass_constant_buffer").Resource.GetGPUVirtualAddress() });
        dx12CommandList->SetGraphicsRootShaderResourceView(2, D3D12_GPU_VIRTUAL_ADDRESS{ m_Buffers.at("material_structured_buffer").Resource.GetGPUVirtualAddress() });
        dx12CommandList->SetGraphicsRootDescriptorTable(3, D3D12_GPU_DESCRIPTOR_HANDLE{ m_Textures.at("space_cubemap").Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
        dx12CommandList->SetGraphicsRootDescriptorTable(4, D3D12_GPU_DESCRIPTOR_HANDLE{ m_Textures.at("red").Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

        for (const auto& renderItem : renderItems)
        {
            if (!renderItem->MeshGeometry || !renderItem->SubmeshGeometry)
            {
                continue;
            }

            dx12CommandList->SetGraphicsRootShaderResourceView(
                1,
                D3D12_GPU_VIRTUAL_ADDRESS
                {
                    m_Buffers.at("render_item_structured_buffer").Resource.GetGPUVirtualAddressWithOffset(renderItem->StructuredBufferOffset)
                }
            );

            context.IASetVertexBuffer(&renderItem->MeshGeometry->VertexBuffer.Views.GetView<spieler::renderer::VertexBufferView>());
            context.IASetIndexBuffer(&renderItem->MeshGeometry->IndexBuffer.Views.GetView<spieler::renderer::IndexBufferView>());
            context.IASetPrimitiveTopology(renderItem->PrimitiveTopology);

            dx12CommandList->DrawIndexedInstanced(
                renderItem->SubmeshGeometry->IndexCount,
                renderItem->VisibleInstanceCount,
                renderItem->SubmeshGeometry->StartIndexLocation,
                renderItem->SubmeshGeometry->BaseVertexLocation,
                0
            );
        }
    }

} // namespace sandbox
