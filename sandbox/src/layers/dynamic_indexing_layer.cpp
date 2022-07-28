#include "bootstrap.hpp"

#include "dynamic_indexing_layer.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/math/math.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/core/application.hpp>
#include <spieler/core/logger.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/upload_buffer.hpp>
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

        m_CameraController = ProjectionCameraController{ spieler::math::ToRadians(60.0f), window.GetAspectRatio() };

        spieler::renderer::UploadBuffer uploadBuffer{ device, spieler::MB(10) };

        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        SPIELER_ASSERT(context.ResetCommandList());

        // Init Textures
        {
            // Crate
            {
                auto& texture{ m_Textures["crate"] };
                texture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/wood_crate1.dds", uploadBuffer);
                texture.SetView<spieler::renderer::ShaderResourceView>(device);
            }

            // Bricks
            {
                auto& texture{ m_Textures["bricks"] };
                texture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/bricks.dds", uploadBuffer);
                texture.SetView<spieler::renderer::ShaderResourceView>(device);
            }

            // Stone
            {
                auto& texture{ m_Textures["stone"] };
                texture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/stone.dds", uploadBuffer);
                texture.SetView<spieler::renderer::ShaderResourceView>(device);
            }

            // Tile
            {
                auto& texture{ m_Textures["tile"] };
                texture.GetTexture2DResource().LoadFromDDSFile(device, context, L"assets/textures/tile.dds", uploadBuffer);
                texture.SetView<spieler::renderer::ShaderResourceView>(device);
            }
        }

        // Init ConstantBuffers
        {
            using namespace magic_enum::bitwise_operators;

            const spieler::renderer::BufferFlags constantBufferFlags{ spieler::renderer::BufferFlags::Dynamic | spieler::renderer::BufferFlags::ConstantBuffer };

            // Pass
            {
                const spieler::renderer::BufferConfig bufferConfig
                {
                    .ElementSize{ sizeof(PassConstants) },
                    .ElementCount{ 1 }
                };

                m_PassConstantBuffer.SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
                m_PassConstantBuffer.SetSlice(&m_PassConstants);
            }

            // Object
            {
                const spieler::renderer::BufferConfig objectBufferConfig
                {
                    .ElementSize{ sizeof(ObjectConstants) },
                    .ElementCount{ 22 }
                };

                m_ObjectConstantBuffer.SetResource(device.CreateBuffer(objectBufferConfig, constantBufferFlags));
            }
            
            // Material
            {
                const spieler::renderer::BufferConfig config
                {
                    .ElementSize{ sizeof(MaterialConstants) },
                    .ElementCount{ 4 }
                };

                m_MaterialConstantBuffer.SetResource(device.CreateBuffer(config, spieler::renderer::BufferFlags::Dynamic));
            }
        }

        // Init Materials
        {
            // Crate
            {
                auto& material{ m_MaterialConstants["crate"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                material.Roughness = 0.2f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 0;

                m_MaterialConstantBuffer.SetSlice(&material);
            }

            // Bricks
            {
                auto& material{ m_MaterialConstants["bricks"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.1f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 1;

                m_MaterialConstantBuffer.SetSlice(&material);
            }

            // Stone
            {
                auto& material{ m_MaterialConstants["stone"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.05f, 0.05f, 0.05f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixIdentity();
                material.DiffuseMapIndex = 2;

                m_MaterialConstantBuffer.SetSlice(&material);
            }

            // Tile
            {
                auto& material{ m_MaterialConstants["tile"] };
                material.DiffuseAlbedo = DirectX::XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f };
                material.FresnelR0 = DirectX::XMFLOAT3{ 0.02f, 0.02f, 0.02f };
                material.Roughness = 0.3f;
                material.Transform = DirectX::XMMatrixScaling(10.0f, 10.0f, 10.0f);
                material.DiffuseMapIndex = 3;

                m_MaterialConstantBuffer.SetSlice(&material);
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
                const spieler::renderer::BufferConfig config
                {
                    .ElementSize{ sizeof(spieler::renderer::Vertex) },
                    .ElementCount{ static_cast<uint32_t>(vertexCount) }
                };

                m_MeshGeometry.VertexBuffer.SetResource(device.CreateBuffer(config, spieler::renderer::BufferFlags::None));
                context.WriteToBuffer(*m_MeshGeometry.VertexBuffer.GetResource(), sizeof(spieler::renderer::Vertex) * vertexCount, vertices.data());
            }
            
            // IndexBuffer
            {
                const spieler::renderer::BufferConfig config
                {
                    .ElementSize{ sizeof(uint32_t) },
                    .ElementCount{ static_cast<uint32_t>(indexCount) }
                };

                m_MeshGeometry.IndexBuffer.SetResource(device.CreateBuffer(config, spieler::renderer::BufferFlags::None));
                context.WriteToBuffer(*m_MeshGeometry.IndexBuffer.GetResource(), sizeof(uint32_t) * indexCount, indices.data());
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

                box->Transform.Scale = DirectX::XMFLOAT3{ 2.0f, 2.0f, 2.0f };

                box->GetComponent<ObjectMaterial>().MaterialIndex = 0;

                m_ObjectConstantBuffer.SetSlice(box.get());
            }

            // Grid
            {
                auto& grid{ m_RenderItems["grid"] = std::make_unique<spieler::renderer::RenderItem>() };
                grid->MeshGeometry = &m_MeshGeometry;
                grid->SubmeshGeometry = &m_MeshGeometry.Submeshes["grid"];
                grid->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                grid->Transform.Scale = DirectX::XMFLOAT3{ 40.0f, 40.0f, 40.0f };
                grid->Transform.Translation = DirectX::XMFLOAT3{ 0.0f, -4.0f, 0.0f };

                grid->GetComponent<ObjectMaterial>().MaterialIndex = 3;

                m_ObjectConstantBuffer.SetSlice(grid.get());
            }

            for (int i = 0; i < 10; ++i)
            {
                // Geosphere
                {
                    auto& Geosphere{ m_RenderItems["geosphere" + std::to_string(i)] = std::make_unique<spieler::renderer::RenderItem>() };
                    Geosphere->MeshGeometry = &m_MeshGeometry;
                    Geosphere->SubmeshGeometry = &m_MeshGeometry.Submeshes["geosphere"];
                    Geosphere->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                    Geosphere->Transform.Translation = DirectX::XMFLOAT3{ (static_cast<float>(i % 2) * 2.0f - 1.0f) * 10.0f, 1.6f, (static_cast<float>(i / 2) - 2.0f) * 5.0f };

                    Geosphere->GetComponent<ObjectMaterial>().MaterialIndex = 2;

                    m_ObjectConstantBuffer.SetSlice(Geosphere.get());
                }
                    
                // Cylinder
                {
                    auto& cylinder{ m_RenderItems["cylinder" + std::to_string(i)] = std::make_unique<spieler::renderer::RenderItem>() };
                    cylinder->MeshGeometry = &m_MeshGeometry;
                    cylinder->SubmeshGeometry = &m_MeshGeometry.Submeshes["cylinder"];
                    cylinder->PrimitiveTopology = spieler::renderer::PrimitiveTopology::TriangleList;

                    cylinder->Transform.Translation = DirectX::XMFLOAT3{ (static_cast<float>(i % 2) * 2.0f - 1.0f) * 10.0f, -2.0f, (static_cast<float>(i / 2) - 2.0f) * 5.0f };

                    cylinder->GetComponent<ObjectMaterial>().MaterialIndex = 1;

                    m_ObjectConstantBuffer.SetSlice(cylinder.get());
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
                .DepthState = spieler::renderer::DepthState
                {
                    .DepthTest{ spieler::renderer::DepthTest::Enabled },
                    .DepthWriteState{ spieler::renderer::DepthWriteState::Enabled },
                    .ComparisonFunction{ spieler::renderer::ComparisonFunction::Less }
                },
                .StencilState = spieler::renderer::StencilState
                {
                    .StencilTest{ spieler::renderer::StencilTest::Disabled },
                }
            };

            const spieler::renderer::InputLayout inputLayout
            {
                spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Normal", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "Tangent", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT3) },
                spieler::renderer::InputLayoutElement{ "TexCoord", spieler::renderer::GraphicsFormat::R32G32Float, sizeof(DirectX::XMFLOAT2) }
            };
            
            const spieler::renderer::GraphicsPipelineStateConfig config
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

        SPIELER_ASSERT(context.ExecuteCommandList(true));

        return true;
    }

    void DynamicIndexingLayer::OnEvent(spieler::Event& event)
    {
        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));

        m_CameraController.OnEvent(event);
    }

    void DynamicIndexingLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Update Pass ConstantBuffer
        {
            const ProjectionCamera& camera{ m_CameraController.GetCamera() };

            m_PassConstants.View = DirectX::XMMatrixTranspose(camera.View);
            m_PassConstants.Projection = DirectX::XMMatrixTranspose(camera.Projection);
            m_PassConstants.ViewProjection = DirectX::XMMatrixTranspose(camera.View * camera.Projection);
            m_PassConstants.CameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.EyePosition);

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Update(&m_PassConstants, sizeof(m_PassConstants));
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

                m_ObjectConstantBuffer.GetSlice(renderItem.get()).Update(&constants, sizeof(constants));
            }
        }

        // Material ConstantBuffer
        {
            for (const auto& [name, material] : m_MaterialConstants)
            {
                m_MaterialConstantBuffer.GetSlice(&material).Update(&material, sizeof(MaterialConstants));
            }
        }
    }

    void DynamicIndexingLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& descriptorManager{ renderer.GetDevice().GetDescriptorManager() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto& d3d12CommandList{ context.GetNativeCommandList() };

        auto& backBuffer{ swapChain.GetCurrentBuffer() };

        context.ResetCommandList();
        {
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &backBuffer.GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::RenderTarget }
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &m_DepthStencil.GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::Present },
                .To{ spieler::renderer::ResourceState::DepthWrite }
            });

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            context.SetRenderTarget(backBuffer.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(backBuffer.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetPipelineState(m_PipelineState);
            context.SetGraphicsRootSignature(m_RootSignature);
            context.SetDescriptorHeap(descriptorManager.GetDescriptorHeap(spieler::renderer::DescriptorHeap::Type::SRV));

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Bind(context, 0);
            
            d3d12CommandList->SetGraphicsRootShaderResourceView(2, D3D12_GPU_VIRTUAL_ADDRESS{ m_MaterialConstantBuffer.GetResource()->GetGPUVirtualAddress() });
            d3d12CommandList->SetGraphicsRootDescriptorTable(3, D3D12_GPU_DESCRIPTOR_HANDLE{ m_Textures["crate"].GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });

            for (auto& [name, renderItem] : m_RenderItems)
            {
                m_ObjectConstantBuffer.GetSlice(renderItem.get()).Bind(context, 1);

                context.IASetVertexBuffer(&renderItem->MeshGeometry->VertexBuffer.GetView());
                context.IASetIndexBuffer(&renderItem->MeshGeometry->IndexBuffer.GetView());
                context.IASetPrimitiveTopology(renderItem->PrimitiveTopology);

                context.GetNativeCommandList()->DrawIndexedInstanced(
                    renderItem->SubmeshGeometry->IndexCount,
                    1,
                    renderItem->SubmeshGeometry->StartIndexLocation,
                    renderItem->SubmeshGeometry->BaseVertexLocation,
                    0
                );
            }

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &backBuffer.GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::RenderTarget },
                .To{ spieler::renderer::ResourceState::Present }
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource{ &m_DepthStencil.GetTexture2DResource() },
                .From{ spieler::renderer::ResourceState::DepthWrite },
                .To{ spieler::renderer::ResourceState::Present }
            });
        }
        context.ExecuteCommandList(true);
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

        const spieler::renderer::Texture2DConfig depthStencilConfig
        {
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Format{ m_DepthStencilFormat },
            .Flags{ spieler::renderer::Texture2DFlags::DepthStencil }
        };

        const spieler::renderer::DepthStencilClearValue depthStencilClearValue
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        SPIELER_ASSERT(device.CreateTexture(depthStencilConfig, depthStencilClearValue, m_DepthStencil.GetTexture2DResource()));
        m_DepthStencil.GetTexture2DResource().SetDebugName(L"DepthStencil");
        m_DepthStencil.SetView<spieler::renderer::DepthStencilView>(device);
    }

    bool DynamicIndexingLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        return false;
    }

} // namespace sandbox
