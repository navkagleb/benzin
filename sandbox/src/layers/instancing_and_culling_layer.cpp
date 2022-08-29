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
            DirectX::XMMATRIX View{};
            DirectX::XMMATRIX Projection{};
            DirectX::XMMATRIX ViewProjection{};
            DirectX::XMFLOAT3 CameraPosition{};
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

        enum class InstancingAndCulling {};

    } // namespace per

    bool InstancingAndCullingLayer::OnAttach()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

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

                initTexture("crate", L"assets/textures/wood_crate1.dds");
                initTexture("bricks", L"assets/textures/bricks.dds");
                initTexture("stone", L"assets/textures/stone.dds");
                initTexture("tile", L"assets/textures/tile.dds");
                initTexture("red", L"assets/textures/red.dds");
                initTexture("green", L"assets/textures/green.dds");
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

                size_t vertexCount{ 0 };
                size_t indexCount{ 0 };

                for (const auto& [name, mesh] : meshes)
                {
                    m_MeshGeometry.Submeshes[name] = spieler::renderer::SubmeshGeometry
                    {
                        .IndexCount{ static_cast<uint32_t>(mesh.Indices.size()) },
                        .BaseVertexLocation{ static_cast<uint32_t>(vertexCount) },
                        .StartIndexLocation{ static_cast<uint32_t>(indexCount) }
                    };

                    vertexCount += mesh.Vertices.size();
                    indexCount += mesh.Indices.size();
                }

                std::vector<spieler::renderer::Vertex> vertices;
                vertices.reserve(vertexCount);

                std::vector<uint32_t> indices;
                indices.reserve(indexCount);

                for (const auto& [name, mesh] : meshes)
                {
                    vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
                    indices.insert(indices.end(), mesh.Indices.begin(), mesh.Indices.end());

                    DirectX::BoundingBox::CreateFromPoints(
                        m_MeshGeometry.Submeshes[name].BoundingBox,
                        vertexCount,
                        reinterpret_cast<const DirectX::XMFLOAT3*>(vertices.data()),
                        sizeof(spieler::renderer::Vertex)
                    );
                }

                // VertexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(spieler::renderer::Vertex) },
                        .ElementCount{ static_cast<uint32_t>(vertexCount) },
                    };

                    m_MeshGeometry.VertexBuffer.Resource = spieler::renderer::BufferResource{ device, config };
                    m_MeshGeometry.VertexBuffer.Views.CreateView<spieler::renderer::VertexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.VertexBuffer.Resource, vertices.data(), sizeof(spieler::renderer::Vertex) * vertexCount);
                }

                // IndexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(uint32_t) },
                        .ElementCount{ static_cast<uint32_t>(indexCount) }
                    };

                    m_MeshGeometry.IndexBuffer.Resource = spieler::renderer::BufferResource{ device, config };
                    m_MeshGeometry.IndexBuffer.Views.CreateView<spieler::renderer::IndexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.IndexBuffer.Resource, indices.data(), sizeof(uint32_t) * indexCount);
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

            // RenderItem Buffer
            {
                auto& buffer{ m_Buffers["render_item_structured_buffer"] };

                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(cb::RenderItem) },
                    .ElementCount{ 1000 },
                    .Flags{ spieler::renderer::BufferResource::Flags::Dynamic }
                };

                buffer.Resource = spieler::renderer::BufferResource{ device, config };
            }

            // Material Buffer
            {
                auto& buffer{ m_Buffers["material_structured_buffer"] };

                const spieler::renderer::BufferResource::Config config
                {
                    .ElementSize{ sizeof(cb::Material) },
                    .ElementCount{ 6 },
                    .Flags{ spieler::renderer::BufferResource::Flags::Dynamic }
                };

                buffer.Resource = spieler::renderer::BufferResource{ device, config };
            }
        }

        // Init m_Materials
        {
            // Crate
            {
                auto& material{ m_Materials["crate"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                material.Roughness = 0.2f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 0;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }

            // Bricks
            {
                auto& material{ m_Materials["bricks"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.1f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 1;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }

            // Stone
            {
                auto& material{ m_Materials["stone"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 2;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }

            // Tile
            {
                auto& material{ m_Materials["tile"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f);
                material.DiffuseMapIndex = 3;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }

            // Red
            {
                auto& material{ m_Materials["red"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f);
                material.DiffuseMapIndex = 4;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }

            // Green
            {
                auto& material{ m_Materials["green"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f);
                material.DiffuseMapIndex = 5;
                material.StructuredBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
            }
        }

        // Init m_RenderItems
        {
            // Boxes
            {
                const int32_t boxInRowCount{ 10 };
                const int32_t boxInColumnCount{ 10 };
                const int32_t boxInDepthCount{ 10 };

                RenderItem& renderItem{ m_RenderItems["box"] };
                renderItem.MeshGeometry = &m_MeshGeometry;
                renderItem.SubmeshGeometry = &m_MeshGeometry.Submeshes["box"];
                renderItem.PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                renderItem.Instances.resize(boxInRowCount * boxInColumnCount * boxInDepthCount);

                for (int32_t i = 0; i < boxInRowCount; ++i)
                {
                    for (int32_t j = 0; j < boxInColumnCount; ++j)
                    {
                        for (int32_t k = 0; k < boxInDepthCount; ++k)
                        {
                            const auto index{ static_cast<size_t>((i * boxInRowCount + j) * boxInColumnCount + k) };

                            renderItem.Instances[index].Transform.Translation = DirectX::XMFLOAT3{ static_cast<float>((i - 5) * 10), static_cast<float>((j - 5) * 10), static_cast<float>((k - 5) * 10) };
                            renderItem.Instances[index].Transform.Scale = DirectX::XMFLOAT3{ 3.0f, 3.0f, 3.0f };
                            renderItem.Instances[index].MaterialIndex = spieler::Random::GetInstance().GetIntegral(4, 5);
                        }
                    }
                }
            }
        }

        // Init m_RootSignature
        {
            spieler::renderer::RootSignature::Config config{ 4, 2 };

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
                    .ShaderRegister{ 0, 1 }
                };

                const spieler::renderer::RootParameter::Descriptor materialStructuredBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ShaderResourceView },
                    .ShaderRegister{ 0, 2 }
                };

                const spieler::renderer::RootParameter::SingleDescriptorTable textureTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 6 },
                        .BaseShaderRegister{ 0 }
                    }
                };

                config.RootParameters[0] = passConstantBuffer;
                config.RootParameters[1] = renderItemStructuredBuffer;
                config.RootParameters[2] = materialStructuredBuffer;
                config.RootParameters[3] = textureTable;
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

                pixelShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::InstancingAndCulling>
                {
                    .Filename{ filename },
                    .EntryPoint{ "PS_Main" },
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
        .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
    },
    .StencilState
    {
        .TestState{ spieler::renderer::StencilState::TestState::Disabled },
    }
};

const spieler::renderer::InputLayout inputLayout
{
    spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
    spieler::renderer::InputLayoutElement{ "Normal", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
    spieler::renderer::InputLayoutElement{ "Tangent", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT3) },
    spieler::renderer::InputLayoutElement{ "TexCoord", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
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

m_PipelineState = spieler::renderer::GraphicsPipelineState{ device, config };
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
    }

    void InstancingAndCullingLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Pass ConstantBuffer
        {
            const cb::Pass constants
            {
                .View{ DirectX::XMMatrixTranspose(m_Camera.GetView()) },
                .Projection{ DirectX::XMMatrixTranspose(m_Camera.GetProjection()) },
                .ViewProjection{ DirectX::XMMatrixTranspose(m_Camera.GetViewProjection()) },
                .CameraPosition{ *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition()) }
            };

            m_Buffers["pass_constant_buffer"].Resource.Write(0, &constants, sizeof(constants));
        }

        // RenderItem StructuredBuffer
        {
            for (auto& [name, renderItem] : m_RenderItems)
            {
                uint32_t visibleInstanceCount{ 0 };

                for (size_t i = 0; i < renderItem.Instances.size(); ++i)
                {
                    const DirectX::XMMATRIX world{ renderItem.Instances[i].Transform.GetMatrix() };

                    DirectX::XMVECTOR worldDeterminant{ DirectX::XMMatrixDeterminant(world) };
                    const DirectX::XMMATRIX inverseWorld{ DirectX::XMMatrixInverse(&worldDeterminant, world) };

                    const DirectX::XMMATRIX viewToLocal{ DirectX::XMMatrixMultiply(m_Camera.GetInverseView(), inverseWorld) };

                    DirectX::BoundingFrustum localSpaceFrustum;
                    m_Camera.GetBoundingFrustum().Transform(localSpaceFrustum, viewToLocal);

                    if (!m_IsCullingEnabled || localSpaceFrustum.Contains(renderItem.SubmeshGeometry->BoundingBox) != DirectX::DISJOINT)
                    {
                        const cb::RenderItem constants
                        {
                            .World{ DirectX::XMMatrixTranspose(world) },
                            .MaterialIndex{ renderItem.Instances[i].MaterialIndex }
                        };

                        m_Buffers["render_item_structured_buffer"].Resource.Write(visibleInstanceCount++ * sizeof(constants), &constants, sizeof(constants));
                    }
                }

                renderItem.VisibleInstanceCount = visibleInstanceCount;
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

            context.SetPipelineState(m_PipelineState);
            context.SetGraphicsRootSignature(m_RootSignature);
            context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::renderer::DescriptorHeap::Type::SRV));

            dx12CommandList->SetGraphicsRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS{ m_Buffers["pass_constant_buffer"].Resource.GetGPUVirtualAddress() });
            dx12CommandList->SetGraphicsRootShaderResourceView(1, D3D12_GPU_VIRTUAL_ADDRESS{ m_Buffers["render_item_structured_buffer"].Resource.GetGPUVirtualAddress() });
            dx12CommandList->SetGraphicsRootShaderResourceView(2, D3D12_GPU_VIRTUAL_ADDRESS{ m_Buffers["material_structured_buffer"].Resource.GetGPUVirtualAddress() });
            dx12CommandList->SetGraphicsRootDescriptorTable(3, D3D12_GPU_DESCRIPTOR_HANDLE{ m_Textures["crate"].Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

            for (auto& [name, renderItem] : m_RenderItems)
            {
                context.IASetVertexBuffer(&renderItem.MeshGeometry->VertexBuffer.Views.GetView<spieler::renderer::VertexBufferView>());
                context.IASetIndexBuffer(&renderItem.MeshGeometry->IndexBuffer.Views.GetView<spieler::renderer::IndexBufferView>());
                context.IASetPrimitiveTopology(renderItem.PrimitiveTopology);

                dx12CommandList->DrawIndexedInstanced(
                    renderItem.SubmeshGeometry->IndexCount,
                    renderItem.VisibleInstanceCount,
                    renderItem.SubmeshGeometry->StartIndexLocation,
                    renderItem.SubmeshGeometry->BaseVertexLocation,
                    0
                );
            }

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
        ImGui::Begin("Culling Data");
        {   
            ImGui::Checkbox("Is Culling Enabled", &m_IsCullingEnabled);

            for (const auto& [name, renderItem] : m_RenderItems)
            {
                ImGui::Text("%s visible instances: %d", name.c_str(), renderItem.VisibleInstanceCount);
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
            .Dimension{ spieler::renderer::TextureResource::Dimension::_2D },
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Depth{ 1 },
            .MipCount{ 1 },
            .Format{ ms_DepthStencilFormat },
            .Flags{ spieler::renderer::TextureResource::Flags::DepthStencil }
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

} // namespace sandbox
