#include "bootstrap.hpp"

#include "tessellation_layer.hpp"

#include <benzin/system/event_dispatcher.hpp>

#include <benzin/graphics/rasterizer_state.hpp>
#include <benzin/graphics/mapped_data.hpp>

#include <benzin/engine/geometry_generator.hpp>

#if 0

namespace sandbox
{

    namespace per
    {

        enum class Tessellation {};

        enum class BezierTessellation {};

    } // namespace per

    bool TessellationLayer::OnAttach()
    {
        auto& window{ benzin::Application::GetInstance().GetWindow() };
        auto& renderer{ benzin::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        {
            benzin::Context::CommandListScope commandListScope{ context, true };

            InitViewport();
            InitScissorRect();
            InitDepthStencil();

            // Init ConstantBuffer
            {
                using namespace magic_enum::bitwise_operators;

                // Pass
                {
                    const benzin::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(PassConstants) },
                        .ElementCount{ 1 },
                        .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
                    };

                    m_PassConstantBuffer = benzin::BufferResource::Create(device, config);
                }
            
                // Object
                {
                    const benzin::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(ObjectConstants) },
                        .ElementCount{ 2 },
                        .Flags{ benzin::BufferResource::Flags::ConstantBuffer }
                    };

                    m_ObjectConstantBuffer = benzin::BufferResource::Create(device, config);
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
                    const benzin::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(DirectX::XMFLOAT3) },
                        .ElementCount{ static_cast<uint32_t>(vertices.size()) }
                    };

                    m_MeshGeometry.VertexBuffer.SetBufferResource(benzin::BufferResource::Create(device, config));

                    context.UploadToBuffer(*m_MeshGeometry.VertexBuffer.GetBufferResource(), vertices.data(), sizeof(DirectX::XMFLOAT3) * vertices.size());
                }

                // IndexBuffer
                {
                    const benzin::BufferResource::Config config
                    {
                        .ElementSize{ sizeof(uint16_t) },
                        .ElementCount{ static_cast<uint32_t>(indices.size()) },
                    };

                    m_MeshGeometry.IndexBuffer.SetBufferResource(benzin::BufferResource::Create(device, config));

                    context.UploadToBuffer(*m_MeshGeometry.IndexBuffer.GetBufferResource(), indices.data(), sizeof(uint16_t) * indices.size());
                }

                m_MeshGeometry.Submeshes["quad"] = benzin::SubmeshGeometry
                {
                    .IndexCount{ 4 },
                    .BaseVertexLocation{ 0 },
                    .StartIndexLocation{ 0 },
                };

                m_MeshGeometry.Submeshes["bezier_quad"] = benzin::SubmeshGeometry
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
                quad.PrimitiveTopology = benzin::PrimitiveTopology::ControlPointPatchlist4;
                quad.ConstantBufferIndex = 0;
                quad.Transform.Scale = DirectX::XMFLOAT3{ 30.0f, 30.0f, 30.0f };

                auto& bezierQuad{ m_RenderItems["bezier_quad"] };
                bezierQuad.MeshGeometry = &m_MeshGeometry;
                bezierQuad.SubmeshGeometry = &m_MeshGeometry.Submeshes["bezier_quad"];
                bezierQuad.PrimitiveTopology = benzin::PrimitiveTopology::ControlPointPatchlist16;
                bezierQuad.ConstantBufferIndex = 1;
                bezierQuad.Transform.Translation = DirectX::XMFLOAT3{ 60.0f, 0.0f, 0.0f };
            }

            // Init RoogSignature
            {
                const benzin::RootParameter::Descriptor pass
                {
                    .Type{ benzin::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 0 }
                };

                const benzin::RootParameter::Descriptor object
                {
                    .Type{ benzin::RootParameter::DescriptorType::ConstantBufferView },
                    .ShaderRegister{ 1 }
                };

                benzin::RootSignature::Config rootSignatureConfig{ 2 };
                rootSignatureConfig.RootParameters[0] = pass;
                rootSignatureConfig.RootParameters[1] = object;

                m_RootSignature = benzin::RootSignature{ device, rootSignatureConfig };
            }

            // Init PipelineState
            {
                // Quad PipelineState
                {
                    const benzin::Shader* vs{ nullptr };
                    const benzin::Shader* hs{ nullptr };
                    const benzin::Shader* ds{ nullptr };
                    const benzin::Shader* ps{ nullptr };

                    // Init Shaders
                    {
                        const std::wstring filename{ L"assets/shaders/tessellation.hlsl" };

                        vs = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Vertex, benzin::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "VS_Main" },
                        });

                        hs = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Hull, benzin::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "HS_Main" },
                        });

                        ds = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Domain, benzin::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "DS_Main" },
                        });

                        ps = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Pixel, benzin::ShaderConfig<per::Tessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "PS_Main" },
                        });
                    }

                    const benzin::RasterizerState rasterizerState
                    {
                        .FillMode{ benzin::FillMode::Wireframe },
                        .CullMode{ benzin::CullMode::None },
                        .TriangleOrder{ benzin::TriangleOrder::Clockwise }
                    };

                    const benzin::InputLayout inputLayout
                    {
                        benzin::InputLayoutElement{ "Position", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) }
                    };

                    const benzin::GraphicsPipelineState::Config graphicsPipelineStateConfig
                    {
                        .RootSignature{ &m_RootSignature },
                        .VertexShader{ vs },
                        .HullShader{ hs },
                        .DomainShader{ ds },
                        .PixelShader{ ps },
                        .RasterizerState{ &rasterizerState },
                        .InputLayout{ &inputLayout },
                        .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Patch },
                        .RTVFormat{ swapChain.GetBufferFormat() },
                        .DSVFormat{ m_DepthStencilFormat }
                    };

                    m_PipelineStates["quad"] = benzin::GraphicsPipelineState{ device, graphicsPipelineStateConfig };
                }
            
                // Bezier quad PipelineState
                {
                    const benzin::Shader* vs{ nullptr };
                    const benzin::Shader* hs{ nullptr };
                    const benzin::Shader* ds{ nullptr };
                    const benzin::Shader* ps{ nullptr };

                    // Init Shaders
                    {
                        const std::wstring filename{ L"assets/shaders/bezier_tessellation.hlsl" };

                        vs = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Vertex, benzin::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "VS_Main" },
                        });

                        hs = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Hull, benzin::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "HS_Main" },
                        });

                        ds = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Domain, benzin::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "DS_Main" },
                        });

                        ps = &m_ShaderLibrary.CreateShader(benzin::ShaderType::Pixel, benzin::ShaderConfig<per::BezierTessellation>
                        {
                            .Filename{ filename },
                            .EntryPoint{ "PS_Main" },
                        });
                    }

                    const benzin::RasterizerState rasterizerState
                    {
                        .FillMode{ benzin::FillMode::Wireframe },
                        .CullMode{ benzin::CullMode::None },
                        .TriangleOrder{ benzin::TriangleOrder::Clockwise }
                    };

                    const benzin::InputLayout inputLayout
                    {
                        benzin::InputLayoutElement{ "Position", benzin::GraphicsFormat::R32G32B32Float, sizeof(DirectX::XMFLOAT3) }
                    };

                    const benzin::GraphicsPipelineState::Config graphicsPipelineStateConfig
                    {
                        .RootSignature{ &m_RootSignature },
                        .VertexShader{ vs },
                        .HullShader{ hs },
                        .DomainShader{ ds },
                        .PixelShader{ ps },
                        .RasterizerState{ &rasterizerState },
                        .InputLayout{ &inputLayout },
                        .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Patch },
                        .RTVFormat{ swapChain.GetBufferFormat() },
                        .DSVFormat{ m_DepthStencilFormat }
                    };

                    m_PipelineStates["bezier_quad"] = benzin::GraphicsPipelineState{ device, graphicsPipelineStateConfig };
                }
            }
        }

        return true;
    }

    void TessellationLayer::OnEvent(benzin::Event& event)
    {
        m_CameraController.OnEvent(event);

        benzin::EventDispatcher dispatcher{ event };
        dispatcher.Dispatch<benzin::WindowResizedEvent>(BENZIN_BIND_EVENT_CALLBACK(OnWindowResized));
    }

    void TessellationLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Update Pass ConstantBuffer
        {
            benzin::MappedData bufferData{ m_PassConstantBuffer, 0 };

            m_PassConstants.View = DirectX::XMMatrixTranspose(m_Camera.GetView());
            m_PassConstants.Projection = DirectX::XMMatrixTranspose(m_Camera.GetProjection());
            m_PassConstants.ViewProjection = DirectX::XMMatrixTranspose(m_Camera.GetViewProjection());
            m_PassConstants.CameraPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&m_Camera.GetPosition());

            bufferData.Write(&m_PassConstants, sizeof(m_PassConstants));
        }

        // Update Object ConstantBuffer
        {
            benzin::MappedData bufferData{ m_ObjectConstantBuffer, 0 };

            for (const auto& [name, renderItem] : m_RenderItems)
            {
                const ObjectConstants constants
                {
                    .World{ DirectX::XMMatrixTranspose(renderItem.Transform.GetMatrix()) }
                };

                bufferData.Write(&constants, sizeof(constants), renderItem.ConstantBufferIndex * m_ObjectConstantBuffer->GetConfig().ElementSize);
            }
        }
    }

    void TessellationLayer::OnRender(float dt)
    {
        auto& renderer{ benzin::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto& backBuffer{ swapChain.GetCurrentBuffer() };

        {
            benzin::Context::CommandListScope commandListScope{ context, true };

            context.SetResourceBarrier(benzin::TransitionResourceBarrier
            {
                .Resource{ backBuffer.GetTextureResource().get() },
                .From{ benzin::Resource::State::Present },
                .To{ benzin::Resource::State::RenderTarget }
            });

            context.SetResourceBarrier(benzin::TransitionResourceBarrier
            {
                .Resource{ m_DepthStencil.GetTextureResource().get() },
                .From{ benzin::Resource::State::Present },
                .To{ benzin::Resource::State::DepthWrite }
            });

            context.SetRenderTarget(backBuffer.GetView<benzin::TextureRenderTargetView>(), m_DepthStencil.GetView<benzin::TextureDepthStencilView>());

            context.ClearRenderTarget(backBuffer.GetView<benzin::TextureRenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.GetView<benzin::TextureDepthStencilView>(), 1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

            // Quad
            {
                const auto& quad{ m_RenderItems["quad"] };

                context.SetPipelineState(m_PipelineStates["quad"]);
                context.SetGraphicsRootSignature(m_RootSignature);

                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(0, m_PassConstantBuffer->GetGPUVirtualAddress());
                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    1, 
                    m_ObjectConstantBuffer->GetGPUVirtualAddress() + quad.ConstantBufferIndex * m_ObjectConstantBuffer->GetConfig().ElementSize
                );

                context.IASetVertexBuffer(quad.MeshGeometry->VertexBuffer.GetBufferResource().get());
                context.IASetIndexBuffer(quad.MeshGeometry->IndexBuffer.GetBufferResource().get());
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

                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(0, m_PassConstantBuffer->GetGPUVirtualAddress());
                context.GetDX12GraphicsCommandList()->SetGraphicsRootConstantBufferView(
                    1,
                    m_ObjectConstantBuffer->GetGPUVirtualAddress() + bezierQuad.ConstantBufferIndex * m_ObjectConstantBuffer->GetConfig().ElementSize
                );

                context.IASetVertexBuffer(bezierQuad.MeshGeometry->VertexBuffer.GetBufferResource().get());
                context.IASetIndexBuffer(bezierQuad.MeshGeometry->IndexBuffer.GetBufferResource().get());
                context.IASetPrimitiveTopology(bezierQuad.PrimitiveTopology);

                context.GetDX12GraphicsCommandList()->DrawIndexedInstanced(
                    bezierQuad.SubmeshGeometry->IndexCount,
                    1,
                    bezierQuad.SubmeshGeometry->StartIndexLocation,
                    bezierQuad.SubmeshGeometry->BaseVertexLocation,
                    0
                );
            }
            
            context.SetResourceBarrier(benzin::TransitionResourceBarrier
            {
                .Resource{ backBuffer.GetTextureResource().get() },
                .From{ benzin::Resource::State::RenderTarget },
                .To{ benzin::Resource::State::Present }
            });

            context.SetResourceBarrier(benzin::TransitionResourceBarrier
            {
                .Resource{ m_DepthStencil.GetTextureResource().get() },
                .From{ benzin::Resource::State::DepthWrite },
                .To{ benzin::Resource::State::Present }
            });
        }
    }

    void TessellationLayer::InitViewport()
    {
        auto& window{ benzin::Application::GetInstance().GetWindow() };

        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(window.GetWidth());
        m_Viewport.Height = static_cast<float>(window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TessellationLayer::InitScissorRect()
    {
        auto& window{ benzin::Application::GetInstance().GetWindow() };

        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(window.GetHeight());
    }

    void TessellationLayer::InitDepthStencil()
    {
        auto& window{ benzin::Application::GetInstance().GetWindow() };
        auto& device{ benzin::Renderer::GetInstance().GetDevice() };

        const benzin::TextureResource::Config config
        {
            .Width{ window.GetWidth() },
            .Height{ window.GetHeight() },
            .Format{ m_DepthStencilFormat },
            .UsageFlags{ benzin::TextureResource::UsageFlags::DepthStencil }
        };

        const benzin::TextureResource::ClearDepthStencil clearDepthStencil
        {
            .Depth{ 1.0f },
            .Stencil{ 0 }
        };

        m_DepthStencil.SetTextureResource(benzin::TextureResource::Create(device, config, clearDepthStencil));
        m_DepthStencil.PushView<benzin::TextureDepthStencilView>(device);
    }

    bool TessellationLayer::OnWindowResized(benzin::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        return false;
    }

} // namespace sandbox

#endif
