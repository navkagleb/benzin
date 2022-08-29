#include "bootstrap.hpp"

#include "dynamic_indexing_layer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/math/math.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/core/application.hpp>
#include <spieler/core/logger.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/rasterizer_state.hpp>
#include <spieler/renderer/depth_stencil_state.hpp> 

namespace sandbox
{

    namespace per
    {

        enum class DynamicIndexing {};

    } // namespace per

    struct ObjectMaterial
    {
        uint32_t MaterialIndex{ 0 };
    };

    bool DynamicIndexingLayer::OnAttach()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            // Init Textures
            {
                // Crate
                {
                    auto& texture{ m_Textures["crate"] };
                    auto subresources{ texture.Resource.LoadFromDDSFile(device, L"assets/textures/wood_crate1.dds") };

                    context.UploadToTexture(texture.Resource, subresources);

                    texture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
                }

                // Bricks
                {
                    auto& texture{ m_Textures["bricks"] };
                    auto subresources{ texture.Resource.LoadFromDDSFile(device, L"assets/textures/bricks.dds") };

                    context.UploadToTexture(texture.Resource, subresources);

                    texture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
                }

                // Stone
                {
                    auto& texture{ m_Textures["stone"] };
                    auto subresources{ texture.Resource.LoadFromDDSFile(device, L"assets/textures/stone.dds") };

                    context.UploadToTexture(texture.Resource, subresources);

                    texture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
                }

                // Tile
                {
                    auto& texture{ m_Textures["tile"] };
                    auto subresources{ texture.Resource.LoadFromDDSFile(device, L"assets/textures/tile.dds") };

                    context.UploadToTexture(texture.Resource, subresources);

                    texture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
                }
            }

            // Init ConstantBuffers
            {
                // Pass
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(PassConstants) },
                        .ElementCount{ 1 },
                        .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
                    };

                    m_PassConstantBuffer = spieler::renderer::BufferResource{ device, config };
                }

                // Object
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(ObjectConstants) },
                        .ElementCount{ 22 },
                        .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
                    };

                    m_ObjectConstantBuffer = spieler::renderer::BufferResource{ device, config };
                }
            
                // Material
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(MaterialConstants) },
                        .ElementCount{ 4 },
                        .Flags{ spieler::renderer::BufferResource::Flags::Dynamic }
                    };
                
                    m_MaterialConstantBuffer = spieler::renderer::BufferResource{ device, config };
                }
            }

            // Init Materials
            {
                // Crate
                {
                    auto& material{ m_Materials["crate"] };
                    material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                    material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                    material.Roughness = 0.2f;
                    material.Transform = DirectX::XMMatrixIdentity();
                    material.DiffuseMapIndex = 0;
                    material.ConstantBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
                }

                // Bricks
                {
                    auto& material{ m_Materials["bricks"] };
                    material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                    material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                    material.Roughness = 0.1f;
                    material.Transform = DirectX::XMMatrixIdentity();
                    material.DiffuseMapIndex = 1;
                    material.ConstantBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
                }

                // Stone
                {
                    auto& material{ m_Materials["stone"] };
                    material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                    material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                    material.Roughness = 0.3f;
                    material.Transform = DirectX::XMMatrixIdentity();
                    material.DiffuseMapIndex = 2;
                    material.ConstantBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
                }

                // Tile
                {
                    auto& material{ m_Materials["tile"] };
                    material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                    material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                    material.Roughness = 0.3f;
                    material.Transform = DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f);
                    material.DiffuseMapIndex = 3;
                    material.ConstantBufferIndex = static_cast<uint32_t>(m_Materials.size() - 1);
                }
            }

            // Init MeshGeometry
            {
                const spieler::renderer::GridGeometryProps gridProps
                {
                    .Width{ 1.0f },
                    .Depth{ 1.0f },
                    .RowCount{ 2 },
                    .ColumnCount{ 2 }
                };

                const spieler::renderer::BoxGeometryProps boxProps
                {
                    .Width{ 1.0f },
                    .Height{ 1.0f },
                    .Depth{ 1.0f }
                };

                const spieler::renderer::GeosphereGeometryProps geosphereProps
                {
                    .Radius{ 1.0f }
                };

                const spieler::renderer::CylinderGeometryProps cylinderProps
                {
                    .TopRadius{ 1.0f },
                    .BottomRadius{ 2.0f },
                    .Height{ 5.0f },
                    .SliceCount{ 20 },
                    .StackCount{ 5 }
                };

                const std::unordered_map<std::string, spieler::renderer::MeshData<uint32_t>> meshes
                {
                    { "grid", spieler::renderer::GeometryGenerator::GenerateGrid<uint32_t>(gridProps) },
                    { "box", spieler::renderer::GeometryGenerator::GenerateBox<uint32_t>(boxProps) },
                    { "geosphere", spieler::renderer::GeometryGenerator::GenerateGeosphere<uint32_t>(geosphereProps) },
                    { "cylinder", spieler::renderer::GeometryGenerator::GenerateCylinder<uint32_t>(cylinderProps) }
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

                for(const auto& [name, mesh] : meshes)
                {
                    vertices.insert(vertices.end(), mesh.Vertices.begin(), mesh.Vertices.end());
                    indices.insert(indices.end(), mesh.Indices.begin(), mesh.Indices.end());
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

            // Init RenderItems
            {
                // Box
                {
                    auto& box{ m_RenderItems["box"] = std::make_unique<spieler::renderer::RenderItem>() };
                    box->MeshGeometry = &m_MeshGeometry;
                    box->SubmeshGeometry = &m_MeshGeometry.Submeshes["box"];
                    box->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                    box->ConstantBufferIndex = 0;

                    box->Transform.Scale = DirectX::XMFLOAT3{ 2.0f, 2.0f, 2.0f };

                    box->GetComponent<ObjectMaterial>().MaterialIndex = 0;
                }

                // Grid
                {
                    auto& grid{ m_RenderItems["grid"] = std::make_unique<spieler::renderer::RenderItem>() };
                    grid->MeshGeometry = &m_MeshGeometry;
                    grid->SubmeshGeometry = &m_MeshGeometry.Submeshes["grid"];
                    grid->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                    grid->ConstantBufferIndex = 1;

                    grid->Transform.Scale = DirectX::XMFLOAT3{ 40.0f, 40.0f, 40.0f };
                    grid->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, -4.0f, 0.0f };

                    grid->GetComponent<ObjectMaterial>().MaterialIndex = 3;
                }

                for (int i = 0; i < 10; ++i)
                {
                    // Geosphere
                    {
                        auto& geosphere{ m_RenderItems["geosphere" + std::to_string(i)] = std::make_unique<spieler::renderer::RenderItem>() };
                        geosphere->MeshGeometry = &m_MeshGeometry;
                        geosphere->SubmeshGeometry = &m_MeshGeometry.Submeshes["geosphere"];
                        geosphere->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                        geosphere->ConstantBufferIndex = 2 + i;

                        geosphere->Transform.Translation = DirectX::XMFLOAT3{ (static_cast<float>(i % 2) * 2.0f - 1.0f) * 10.0f, 1.6f, (static_cast<float>(i / 2) - 2.0f) * 5.0f };

                        geosphere->GetComponent<ObjectMaterial>().MaterialIndex = 2;
                    }
                    
                    // Cylinder
                    {
                        auto& cylinder{ m_RenderItems["cylinder" + std::to_string(i)] = std::make_unique<spieler::renderer::RenderItem>() };
                        cylinder->MeshGeometry = &m_MeshGeometry;
                        cylinder->SubmeshGeometry = &m_MeshGeometry.Submeshes["cylinder"];
                        cylinder->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                        cylinder->ConstantBufferIndex = 12 + i;

                        cylinder->Transform.Translation = DirectX::XMFLOAT3{ (static_cast<float>(i % 2) * 2.0f - 1.0f) * 10.0f, -2.0f, (static_cast<float>(i / 2) - 2.0f) * 5.0f };

                        cylinder->GetComponent<ObjectMaterial>().MaterialIndex = 1;
                    }
                }
            }

            // Init RootSignature
            {
                const spieler::renderer::RootParameter::Descriptor passConstantBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                const spieler::renderer::RootParameter::Descriptor objectConstantBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 1 }
                };

                const spieler::renderer::RootParameter::Descriptor materialStructuredBuffer
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ShaderResourceView },
                    .ShaderRegister{ 0, 1 }
                };

                const spieler::renderer::RootParameter::SingleDescriptorTable textureTable
                {
                    .Range
                    {
                        .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                        .DescriptorCount{ 4 },
                        .BaseShaderRegister{ 0 }
                    }
                };

                spieler::renderer::RootSignature::Config config{ 4, 2 };
                config.RootParameters[0] = passConstantBuffer;
                config.RootParameters[1] = objectConstantBuffer;
                config.RootParameters[2] = materialStructuredBuffer;
                config.RootParameters[3] = textureTable;

                config.StaticSamplers[0] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilter{ spieler::renderer::TextureFilterType::Linear }, spieler::renderer::TextureAddressMode::Wrap, 0 };
                config.StaticSamplers[1] = spieler::renderer::StaticSampler{ spieler::renderer::TextureFilter{ spieler::renderer::TextureFilterType::Anisotropic }, spieler::renderer::TextureAddressMode::Wrap, 1 };

                m_RootSignature = spieler::renderer::RootSignature{ device, config };
            }

            // Init PipelineState
            {
                const spieler::renderer::Shader* vertexShader{ nullptr };
                const spieler::renderer::Shader* pixelShader{ nullptr };

                // Init Shaders
                {
                    const std::wstring filename{ L"assets/shaders/dynamic_indexing.hlsl" };

                    vertexShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::DynamicIndexing>
                    {
                        .Filename{ filename },
                        .EntryPoint{ "VS_Main" },
                    });

                    pixelShader = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::DynamicIndexing>
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
                    .DSVFormat{ m_DepthStencilFormat }
                };

                m_PipelineState = spieler::renderer::GraphicsPipelineState{ device, config };
            }
        }

        return true;
    }

    void DynamicIndexingLayer::OnEvent(spieler::Event& event)
    {
        m_CameraController.OnEvent(event);

        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
    }

    void DynamicIndexingLayer::OnUpdate(float dt)
    {
        auto& context{ spieler::renderer::Renderer::GetInstance().GetContext() };

        m_CameraController.OnUpdate(dt);

        // Update Pass ConstantBuffer
        {
            m_PassConstants.View = DirectX::XMMatrixTranspose(m_Camera.GetView());
            m_PassConstants.Projection = DirectX::XMMatrixTranspose(m_Camera.GetProjection());
            m_PassConstants.ViewProjection = DirectX::XMMatrixTranspose(m_Camera.GetViewProjection());
            m_PassConstants.CameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition());

            m_PassConstantBuffer.Write(0, &m_PassConstants, sizeof(m_PassConstants));
        }

        // Update Object ConstantBuffer
        {
            for (const auto& [name, renderItem] : m_RenderItems)
            {
                const ObjectConstants constants
                {
                    .World{ DirectX::XMMatrixTranspose(renderItem->Transform.GetMatrix()) },
                    .MaterialIndex{ renderItem->HasComponent<ObjectMaterial>() ? renderItem->GetComponent<ObjectMaterial>().MaterialIndex : 0 }
                };

                const uint64_t offset{ renderItem->ConstantBufferIndex * m_ObjectConstantBuffer.GetConfig().ElementSize };

                m_ObjectConstantBuffer.Write(offset, &constants, sizeof(constants));
            }
        }

        // Material ConstantBuffer
        {
            for (const auto& [name, material] : m_Materials)
            {
                const MaterialConstants constants
                {
                    .DiffuseAlbedo{ material.DiffuseAlbedo },
                    .FresnelR0{ material.FresnelR0 },
                    .Roughness{ material.Roughness },
                    .Transform{ material.Transform },
                    .DiffuseMapIndex{ material.DiffuseMapIndex },
                };

                m_MaterialConstantBuffer.Write(material.ConstantBufferIndex * sizeof(constants), &constants, sizeof(constants));
            }
        }
    }

    void DynamicIndexingLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& descriptorManager{ renderer.GetDevice().GetDescriptorManager() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto* d3d12CommandList{ context.GetDX12GraphicsCommandList() };

        auto& backBuffer{ swapChain.GetCurrentBuffer() };

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
                .Resource{ &m_DepthStencil.Resource },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::DepthWrite }
            });

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            context.SetRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.Views.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.Views.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetPipelineState(m_PipelineState);
            context.SetGraphicsRootSignature(m_RootSignature);
            context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::renderer::DescriptorHeap::Type::SRV));

            d3d12CommandList->SetGraphicsRootConstantBufferView(0, D3D12_GPU_VIRTUAL_ADDRESS{ m_PassConstantBuffer.GetGPUVirtualAddress() });
            d3d12CommandList->SetGraphicsRootShaderResourceView(2, D3D12_GPU_VIRTUAL_ADDRESS{ m_MaterialConstantBuffer.GetGPUVirtualAddress() });
            d3d12CommandList->SetGraphicsRootDescriptorTable(3, D3D12_GPU_DESCRIPTOR_HANDLE{ m_Textures["crate"].Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

            for (auto& [name, renderItem] : m_RenderItems)
            {
                d3d12CommandList->SetGraphicsRootConstantBufferView(1, D3D12_GPU_VIRTUAL_ADDRESS
                {
                    m_ObjectConstantBuffer.GetGPUVirtualAddress() + static_cast<uint64_t>(renderItem->ConstantBufferIndex) * m_ObjectConstantBuffer.GetConfig().ElementSize
                });

                context.IASetVertexBuffer(&renderItem->MeshGeometry->VertexBuffer.Views.GetView<spieler::renderer::VertexBufferView>());
                context.IASetIndexBuffer(&renderItem->MeshGeometry->IndexBuffer.Views.GetView<spieler::renderer::IndexBufferView>());
                context.IASetPrimitiveTopology(renderItem->PrimitiveTopology);

                context.GetDX12GraphicsCommandList()->DrawIndexedInstanced(
                    renderItem->SubmeshGeometry->IndexCount,
                    1,
                    renderItem->SubmeshGeometry->StartIndexLocation,
                    renderItem->SubmeshGeometry->BaseVertexLocation,
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
                .Resource{ &m_DepthStencil.Resource },
                .From{ spieler::renderer::ResourceState::DepthWrite },
                .To{ spieler::renderer::ResourceState::Present }
            });
        }
    }

    void DynamicIndexingLayer::InitViewport()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(window.GetWidth());
        m_Viewport.Height = static_cast<float>(window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void DynamicIndexingLayer::InitScissorRect()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(window.GetHeight());
    }

    void DynamicIndexingLayer::InitDepthStencil()
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
            .Format{ m_DepthStencilFormat },
            .Flags{ spieler::renderer::TextureResource::Flags::DepthStencil }
        };

        const spieler::renderer::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        m_DepthStencil.Resource = spieler::renderer::TextureResource{ device, config, clearDepthStencil };

        m_DepthStencil.Views.Clear();
        m_DepthStencil.Views.CreateView<spieler::renderer::DepthStencilView>(device);
    }

    bool DynamicIndexingLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        return false;
    }

} // namespace sandbox
