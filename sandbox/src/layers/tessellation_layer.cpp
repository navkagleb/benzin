#include "bootstrap.hpp"

#include "tessellation_layer.hpp"

#include <spieler/core/application.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/rasterizer_state.hpp>

namespace sandbox
{

    namespace per
    {

        enum class Tessellation {};

        enum class BezierTessellation {};

    } // namespace per

    bool TessellationLayer::OnAttach()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        m_CameraController = ProjectionCameraController{ spieler::math::ToRadians(60.0f), window.GetAspectRatio() };

        {
            spieler::renderer::Context::CommandListScope commandListScope{ context, true };

            InitViewport();
            InitScissorRect();
            InitDepthStencil();

            // Init ConstantBuffer
            {
                using namespace magic_enum::bitwise_operators;

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
                        .ElementCount{ 2 },
                        .Flags{ spieler::renderer::BufferResource::Flags::ConstantBuffer }
                    };

                    m_ObjectConstantBuffer = spieler::renderer::BufferResource{ device, config };
                }
            }

            // Init MeshGeometry
            {
                const std::array<DirectX::XMFLOAT3, 4 + 16> vertices
                {
                    // Quad
                    DirectX::XMFLOAT3{ -0.5f, 0.0f,  0.5f },
                    DirectX::XMFLOAT3{  0.5f, 0.0f,  0.5f },
                    DirectX::XMFLOAT3{ -0.5f, 0.0f, -0.5f },
                    DirectX::XMFLOAT3{  0.5f, 0.0f, -0.5f },

                    // Bezier Quad
                    // Row 0
                    DirectX::XMFLOAT3{ -10.0f, -10.0f, +15.0f },
                    DirectX::XMFLOAT3{ -5.0f,  0.0f, +15.0f },
                    DirectX::XMFLOAT3{ +5.0f,  0.0f, +15.0f },
                    DirectX::XMFLOAT3{ +10.0f, 0.0f, +15.0f },

                    // Row 1
                    DirectX::XMFLOAT3{ -15.0f, 0.0f, +5.0f },
                    DirectX::XMFLOAT3{ -5.0f,  0.0f, +5.0f },
                    DirectX::XMFLOAT3{ +5.0f,  20.0f, +5.0f },
                    DirectX::XMFLOAT3{ +15.0f, 0.0f, +5.0f },

                    // Row 2
                    DirectX::XMFLOAT3{ -15.0f, 0.0f, -5.0f },
                    DirectX::XMFLOAT3{ -5.0f,  0.0f, -5.0f },
                    DirectX::XMFLOAT3{ +5.0f,  0.0f, -5.0f },
                    DirectX::XMFLOAT3{ +15.0f, 0.0f, -5.0f },

                    // Row 3
                    DirectX::XMFLOAT3{ -10.0f, 10.0f, -15.0f },
                    DirectX::XMFLOAT3{ -5.0f,  0.0f, -15.0f },
                    DirectX::XMFLOAT3{ +5.0f,  0.0f, -15.0f },
                    DirectX::XMFLOAT3{ +25.0f, 10.0f, -15.0f }
                };

                const std::array<int16_t, 4 + 16> indices
                { 
                    // Quad
                    0, 1, 2, 3,

                    // Bezier Quad
                    0, 1, 2, 3,
                    4, 5, 6, 7,
                    8, 9, 10, 11,
                    12, 13, 14, 15
                };

                // VertexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(DirectX::XMFLOAT3) },
                        .ElementCount{ static_cast<uint32_t>(vertices.size()) }
                    };

                    m_MeshGeometry.VertexBuffer.Resource = spieler::renderer::BufferResource{ device, config };
                    m_MeshGeometry.VertexBuffer.Views.CreateView<spieler::renderer::VertexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.VertexBuffer.Resource, vertices.data(), sizeof(DirectX::XMFLOAT3) * vertices.size());
                }

                // IndexBuffer
                {
                    const spieler::renderer::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(uint16_t) },
                        .ElementCount{ static_cast<uint32_t>(indices.size()) },
                    };

                    m_MeshGeometry.IndexBuffer.Resource = spieler::renderer::BufferResource{ device, config };;
                    m_MeshGeometry.IndexBuffer.Views.CreateView<spieler::renderer::IndexBufferView>();

                    context.UploadToBuffer(m_MeshGeometry.IndexBuffer.Resource, indices.data(), sizeof(uint16_t) * indices.size());
                }

                m_MeshGeometry.Submeshes["quad"] = spieler::renderer::SubmeshGeometry
                {
                    .IndexCount{ 4 },
                    .BaseVertexLocation{ 0 },
                    .StartIndexLocation{ 0 },
                };

                m_MeshGeometry.Submeshes["bezier_quad"] = spieler::renderer::SubmeshGeometry
                {
                    .IndexCount{ 16 },
                    .BaseVertexLocation{ 4 },
                    .StartIndexLocation{ 4 },
                };
            }

            // Init RenderItems
            {
                auto& quad{ m_RenderItems["quad"] };
                quad.MeshGeometry = &m_MeshGeometry;
                quad.SubmeshGeometry = &m_MeshGeometry.Submeshes["quad"];
                quad.PrimitiveTopology = spieler::renderer::PrimitiveTopology::ControlPointPatchlist4;
                quad.ConstantBufferIndex = 0;
                quad.Transform.Scale = DirectX::XMFLOAT3{ 30.0f, 30.0f, 30.0f };

                auto& bezierQuad{ m_RenderItems["bezier_quad"] };
                bezierQuad.MeshGeometry = &m_MeshGeometry;
                bezierQuad.SubmeshGeometry = &m_MeshGeometry.Submeshes["bezier_quad"];
                bezierQuad.PrimitiveTopology = spieler::renderer::PrimitiveTopology::ControlPointPatchlist16;
                bezierQuad.ConstantBufferIndex = 1;
                bezierQuad.Transform.Translation = DirectX::XMFLOAT3{ 60.0f, 0.0f, 0.0f };
            }

            // Init RoogSignature
            {
                const spieler::renderer::RootParameter::Descriptor pass
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                const spieler::renderer::RootParameter::Descriptor object
                {
                    .Type{ spieler::renderer::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 1 }
                };

                spieler::renderer::RootSignature::Config rootSignatureConfig{ 2 };
                rootSignatureConfig.RootParameters[0] = pass;
                rootSignatureConfig.RootParameters[1] = object;

                m_RootSignature = spieler::renderer::RootSignature{ device, rootSignatureConfig };
            }

            // Init PipelineState
            {
                // Quad PipelineState
                {
                    const spieler::renderer::Shader* vs{ nullptr };
                    const spieler::renderer::Shader* hs{ nullptr };
                    const spieler::renderer::Shader* ds{ nullptr };
                    const spieler::renderer::Shader* ps{ nullptr };

                    // Init Shaders
                    {
                        const std::wstring filename{ L"assets/shaders/tessellation.hlsl" };

                        vs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "VS_Main" },
                        });

                        hs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Hull, spieler::renderer::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "HS_Main" },
                        });

                        ds = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Domain, spieler::renderer::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "DS_Main" },
                        });

                        ps = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "PS_Main" },
                        });
                    }

                    const spieler::renderer::RasterizerState rasterizerState
                    {
                        .FillMode{ spieler::renderer::FillMode::Wireframe },
                        .CullMode{ spieler::renderer::CullMode::None },
                        .TriangleOrder{ spieler::renderer::TriangleOrder::Clockwise }
                    };

                    const spieler::renderer::InputLayout inputLayout
                    {
                        spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) }
                    };

                    const spieler::renderer::GraphicsPipelineState::Config graphicsPipelineStateConfig
                    {
                        .RootSignature{ &m_RootSignature },
                        .VertexShader{ vs },
                        .HullShader{ hs },
                        .DomainShader{ ds },
                        .PixelShader{ ps },
                        .RasterizerState{ &rasterizerState },
                        .InputLayout{ &inputLayout },
                        .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Patch },
                        .RTVFormat{ swapChain.GetBufferFormat() },
                        .DSVFormat{ m_DepthStencilFormat }
                    };

                    m_PipelineStates["quad"] = spieler::renderer::GraphicsPipelineState{ device, graphicsPipelineStateConfig };
                }
            
                // Bezier quad PipelineState
                {
                    const spieler::renderer::Shader* vs{ nullptr };
                    const spieler::renderer::Shader* hs{ nullptr };
                    const spieler::renderer::Shader* ds{ nullptr };
                    const spieler::renderer::Shader* ps{ nullptr };

                    // Init Shaders
                    {
                        const std::wstring filename{ L"assets/shaders/bezier_tessellation.hlsl" };

                        vs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "VS_Main" },
                        });

                        hs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Hull, spieler::renderer::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "HS_Main" },
                        });

                        ds = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Domain, spieler::renderer::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "DS_Main" },
                        });

                        ps = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "PS_Main" },
                        });
                    }

                    const spieler::renderer::RasterizerState rasterizerState
                    {
                        .FillMode{ spieler::renderer::FillMode::Wireframe },
                        .CullMode{ spieler::renderer::CullMode::None },
                        .TriangleOrder{ spieler::renderer::TriangleOrder::Clockwise }
                    };

                    const spieler::renderer::InputLayout inputLayout
                    {
                        spieler::renderer::InputLayoutElement{ "Position", spieler::renderer::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) }
                    };

                    const spieler::renderer::GraphicsPipelineState::Config graphicsPipelineStateConfig
                    {
                        .RootSignature{ &m_RootSignature },
                        .VertexShader{ vs },
                        .HullShader{ hs },
                        .DomainShader{ ds },
                        .PixelShader{ ps },
                        .RasterizerState{ &rasterizerState },
                        .InputLayout{ &inputLayout },
                        .PrimitiveTopologyType{ spieler::renderer::PrimitiveTopologyType::Patch },
                        .RTVFormat{ swapChain.GetBufferFormat() },
                        .DSVFormat{ m_DepthStencilFormat }
                    };

                    m_PipelineStates["bezier_quad"] = spieler::renderer::GraphicsPipelineState{ device, graphicsPipelineStateConfig };
                }
            }
        }

        return true;
    }

    void TessellationLayer::OnEvent(spieler::Event& event)
    {
        m_CameraController.OnEvent(event);

        spieler::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<spieler::WindowResizedEvent>(SPIELER_BIND_EVENT_CALLBACK(OnWindowResized));
    }

    void TessellationLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Update Pass ConstantBuffer
        {
            const ProjectionCamera& camera{ m_CameraController.GetCamera() };

            m_PassConstants.View = DirectX::XMMatrixTranspose(camera.View);
            m_PassConstants.Projection = DirectX::XMMatrixTranspose(camera.Projection);
            m_PassConstants.ViewProjection = DirectX::XMMatrixTranspose(camera.View * camera.Projection);
            m_PassConstants.CameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.EyePosition);

            m_PassConstantBuffer.Write(0, &m_PassConstants, sizeof(m_PassConstants));
        }

        // Update Object ConstantBuffer
        {
            for (const auto& [name, renderItem] : m_RenderItems)
            {
                const ObjectConstants constants
                {
                    .World{ DirectX::XMMatrixTranspose(renderItem.Transform.GetMatrix()) }
                };

                const uint64_t offset{ renderItem.ConstantBufferIndex * m_ObjectConstantBuffer.GetConfig().ElementSize };

                m_ObjectConstantBuffer.Write(offset, &constants, sizeof(constants));
            }
        }
    }

    void TessellationLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

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

            context.SetRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.Views.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(backBuffer.Views.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.Views.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            // Quad
            {
                const auto& quad{ m_RenderItems["quad"] };

                context.SetPipelineState(m_PipelineStates["quad"]);
                context.SetGraphicsRootSignature(m_RootSignature);

                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(0, m_PassConstantBuffer.GetGPUVirtualAddress());
                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    1, 
                    m_ObjectConstantBuffer.GetGPUVirtualAddress() + quad.ConstantBufferIndex * m_ObjectConstantBuffer.GetConfig().ElementSize
                );

                context.IASetVertexBuffer(&quad.MeshGeometry->VertexBuffer.Views.GetView<spieler::renderer::VertexBufferView>());
                context.IASetIndexBuffer(&quad.MeshGeometry->IndexBuffer.Views.GetView<spieler::renderer::IndexBufferView>());
                context.IASetPrimitiveTopology(quad.PrimitiveTopology);

                context.GetDX12GraphicsCommandList()->DrawIndexedInstanced(
                    quad.SubmeshGeometry->IndexCount,
                    1,
                    quad.SubmeshGeometry->StartIndexLocation,
                    quad.SubmeshGeometry->BaseVertexLocation,
                    0
                );
            }

            // Bezier Quad
            {
                const auto& bezierQuad{ m_RenderItems["bezier_quad"] };

                context.SetPipelineState(m_PipelineStates["bezier_quad"]);
                context.SetGraphicsRootSignature(m_RootSignature);

                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(0, m_PassConstantBuffer.GetGPUVirtualAddress());
                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    1,
                    m_ObjectConstantBuffer.GetGPUVirtualAddress() + bezierQuad.ConstantBufferIndex * m_ObjectConstantBuffer.GetConfig().ElementSize
                );

                context.IASetVertexBuffer(&bezierQuad.MeshGeometry->VertexBuffer.Views.GetView<spieler::renderer::VertexBufferView>());
                context.IASetIndexBuffer(&bezierQuad.MeshGeometry->IndexBuffer.Views.GetView<spieler::renderer::IndexBufferView>());
                context.IASetPrimitiveTopology(bezierQuad.PrimitiveTopology);

                context.GetDX12GraphicsCommandList()->DrawIndexedInstanced(
                    bezierQuad.SubmeshGeometry->IndexCount,
                    1,
                    bezierQuad.SubmeshGeometry->StartIndexLocation,
                    bezierQuad.SubmeshGeometry->BaseVertexLocation,
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

    void TessellationLayer::InitViewport()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(window.GetWidth());
        m_Viewport.Height = static_cast<float>(window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TessellationLayer::InitScissorRect()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };

        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(window.GetHeight());
    }

    void TessellationLayer::InitDepthStencil()
    {
        auto& window{ spieler::Application::GetInstance().GetWindow() };
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::TextureResource::Config config
        {
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
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

    bool TessellationLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        return false;
    }

} // namespace sandbox
