#include "bootstrap.hpp"

#include "tessellation_layer.hpp"

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

    TessellationLayer::TessellationLayer(spieler::Window& window)
        : m_Window{ window }
        , m_CameraController{ spieler::math::ToRadians(60.0f), m_Window.GetAspectRatio() }
    {}

    bool TessellationLayer::OnAttach()
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        spieler::renderer::UploadBuffer uploadBuffer{ device, spieler::renderer::UploadBufferType::Default, spieler::MB(2) };

        SPIELER_ASSERT(context.ResetCommandList());

        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        // Init ConstantBuffer
        {
            using namespace magic_enum::bitwise_operators;

            const spieler::renderer::BufferFlags constantBufferFlags{ spieler::renderer::BufferFlags::Dynamic | spieler::renderer::BufferFlags::ConstantBuffer };

            // Pass
            const spieler::renderer::BufferConfig bufferConfig
            {
                .ElementSize{ sizeof(PassConstants) },
                .ElementCount{ 1 }
            };

            m_PassConstantBuffer.SetResource(device.CreateBuffer(bufferConfig, constantBufferFlags));
            m_PassConstantBuffer.SetSlice(&m_PassConstants);

            // Object
            const spieler::renderer::BufferConfig objectBufferConfig
            {
                .ElementSize{ sizeof(ObjectConstants) },
                .ElementCount{ 2 }
            };

            m_RenderItems["quad"].Transform.Scale = DirectX::XMFLOAT3{ 20.0f, 20.0f, 20.0f };

            m_ObjectConstantBuffer.SetResource(device.CreateBuffer(objectBufferConfig, constantBufferFlags));
            m_ObjectConstantBuffer.SetSlice(&m_RenderItems["quad"]);
            m_ObjectConstantBuffer.SetSlice(&m_RenderItems["bezier_quad"]);
        }

        // Init MeshGeometry
        {
            const std::array<DirectX::XMFLOAT3, 4 + 16> vertices =
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

            const std::array<std::int16_t, 4 + 16> indices
            { 
                // Quad
                0, 1, 2, 3,

                // Bezier Quad
                0, 1, 2, 3,
                4, 5, 6, 7,
                8, 9, 10, 11,
                12, 13, 14, 15
            };

            m_MeshGeometry.InitStaticVertexBuffer(vertices.data(), vertices.size(), uploadBuffer);
            m_MeshGeometry.InitStaticIndexBuffer(indices.data(), indices.size(), uploadBuffer);

            m_MeshGeometry.m_PrimitiveTopology = spieler::renderer::PrimitiveTopology::ControlPointPatchlist16;

            spieler::renderer::SubmeshGeometry& grid{ m_MeshGeometry.CreateSubmesh("quad") };
            grid.IndexCount = 4;
            grid.BaseVertexLocation = 0;
            grid.StartIndexLocation = 0;

            spieler::renderer::SubmeshGeometry& bezierQuad{ m_MeshGeometry.CreateSubmesh("bezier_quad") };
            bezierQuad.IndexCount = 16;
            bezierQuad.BaseVertexLocation = 4;
            bezierQuad.StartIndexLocation = 4;
        }

        // Init RenderItems
        {
            auto& quad{ m_RenderItems["quad"] };
            quad.MeshGeometry = &m_MeshGeometry;
            quad.SubmeshGeometry = &m_MeshGeometry.GetSubmesh("quad");

            auto& bezierQuad{ m_RenderItems["bezier_quad"] };
            bezierQuad.MeshGeometry = &m_MeshGeometry;
            bezierQuad.SubmeshGeometry = &m_MeshGeometry.GetSubmesh("bezier_quad");
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

                const spieler::renderer::GraphicsPipelineStateConfig graphicsPipelineStateConfig
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

                const spieler::renderer::GraphicsPipelineStateConfig graphicsPipelineStateConfig
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

        SPIELER_ASSERT(context.ExecuteCommandList(true));

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

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Update(&m_PassConstants, sizeof(m_PassConstants));
        }

        // Update Object ConstantBuffer
        {
            for (const auto& [name, renderItem] : m_RenderItems)
            {
                m_ObjectConstants[name].World = DirectX::XMMatrixTranspose(renderItem.Transform.GetMatrix());

                m_ObjectConstantBuffer.GetSlice(&renderItem).Update(&m_ObjectConstants[name], sizeof(m_ObjectConstants[name]));
            }
        }
    }

    bool TessellationLayer::OnRender(float dt)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& swapChain{ renderer.GetSwapChain() };
        auto& context{ renderer.GetContext() };

        auto& backBuffer{ swapChain.GetCurrentBuffer() };

        SPIELER_ASSERT(context.ResetCommandList());
        {
            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &backBuffer.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::RenderTarget
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &m_DepthStencil.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::Present,
                .To = spieler::renderer::ResourceState::DepthWrite
            });

            context.SetRenderTarget(backBuffer.GetView<spieler::renderer::RenderTargetView>(), m_DepthStencil.GetView<spieler::renderer::DepthStencilView>());

            context.ClearRenderTarget(backBuffer.GetView<spieler::renderer::RenderTargetView>(), { 0.1f, 0.1f, 0.1f, 1.0f });
            context.ClearDepthStencil(m_DepthStencil.GetView<spieler::renderer::DepthStencilView>(), 1.0f, 0);

            context.SetViewport(m_Viewport);
            context.SetScissorRect(m_ScissorRect);

#if 0
            // Quad
            context.SetPipelineState(m_PipelineStates["quad"]);
            context.SetGraphicsRootSignature(m_RootSignature);

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Bind(context, 0);
            m_ObjectConstantBuffer.GetSlice(&m_RenderItems["quad"]).Bind(context, 1);

            m_RenderItems["quad"].MeshGeometry->GetVertexBuffer().GetView().Bind(context);
            m_RenderItems["quad"].MeshGeometry->GetIndexBuffer().GetView().Bind(context);

            context.SetPrimitiveTopology(m_MeshGeometry.GetPrimitiveTopology());

            context.GetNativeCommandList()->DrawIndexedInstanced(
                m_RenderItems["quad"].SubmeshGeometry->IndexCount,
                1,
                m_RenderItems["quad"].SubmeshGeometry->StartIndexLocation,
                m_RenderItems["quad"].SubmeshGeometry->BaseVertexLocation,
                0
            );
#endif

            // Bezier Quad
            context.SetPipelineState(m_PipelineStates["bezier_quad"]);
            context.SetGraphicsRootSignature(m_RootSignature);

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Bind(context, 0);
            m_ObjectConstantBuffer.GetSlice(&m_RenderItems["bezier_quad"]).Bind(context, 1);

            m_RenderItems["bezier_quad"].MeshGeometry->GetVertexBuffer().GetView().Bind(context);
            m_RenderItems["bezier_quad"].MeshGeometry->GetIndexBuffer().GetView().Bind(context);

            context.SetPrimitiveTopology(m_MeshGeometry.GetPrimitiveTopology());

            context.GetNativeCommandList()->DrawIndexedInstanced(
                m_RenderItems["bezier_quad"].SubmeshGeometry->IndexCount,
                1,
                m_RenderItems["bezier_quad"].SubmeshGeometry->StartIndexLocation,
                m_RenderItems["bezier_quad"].SubmeshGeometry->BaseVertexLocation,
                0
            );

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &backBuffer.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::RenderTarget,
                .To = spieler::renderer::ResourceState::Present
            });

            context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
            {
                .Resource = &m_DepthStencil.GetTexture2DResource(),
                .From = spieler::renderer::ResourceState::DepthWrite,
                .To = spieler::renderer::ResourceState::Present
            });
        }
        SPIELER_ASSERT(context.ExecuteCommandList(true));

        return true;
    }

    void TessellationLayer::OnImGuiRender(float dt)
    {

    }

    void TessellationLayer::InitViewport()
    {
        m_Viewport.X = 0.0f;
        m_Viewport.Y = 0.0f;
        m_Viewport.Width = static_cast<float>(m_Window.GetWidth());
        m_Viewport.Height = static_cast<float>(m_Window.GetHeight());
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;
    }

    void TessellationLayer::InitScissorRect()
    {
        m_ScissorRect.X = 0.0f;
        m_ScissorRect.Y = 0.0f;
        m_ScissorRect.Width = static_cast<float>(m_Window.GetWidth());
        m_ScissorRect.Height = static_cast<float>(m_Window.GetHeight());
    }

    void TessellationLayer::InitDepthStencil()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::Texture2DConfig depthStencilConfig
        {
            .Width = m_Window.GetWidth(),
            .Height = m_Window.GetHeight(),
            .Format = m_DepthStencilFormat,
            .Flags = spieler::renderer::Texture2DFlags::DepthStencil
        };

        const spieler::renderer::DepthStencilClearValue depthStencilClearValue
        {
            .Depth = 1.0f,
            .Stencil = 0
        };

        SPIELER_ASSERT(device.CreateTexture(depthStencilConfig, depthStencilClearValue, m_DepthStencil.GetTexture2DResource()));
        m_DepthStencil.GetTexture2DResource().SetDebugName(L"DepthStencil");
        m_DepthStencil.SetView<spieler::renderer::DepthStencilView>(device);
    }

    bool TessellationLayer::OnWindowResized(spieler::WindowResizedEvent& event)
    {
        InitViewport();
        InitScissorRect();
        InitDepthStencil();

        return true;
    }

} // namespace sandbox
