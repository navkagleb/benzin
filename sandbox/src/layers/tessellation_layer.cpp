#include "bootstrap.hpp"

#include "tessellation_layer.hpp"

#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/geometry_generator.hpp>
#include <spieler/renderer/rasterizer_state.hpp>

namespace sandbox
{

    namespace per
    {

        enum class Tessellation {};

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
                .ElementCount{ 1 }
            };

            m_RenderItem.Transform.Scale = DirectX::XMFLOAT3{ 10.0f, 10.0f, 10.0f };

            m_ObjectConstantBuffer.SetResource(device.CreateBuffer(objectBufferConfig, constantBufferFlags));
            m_ObjectConstantBuffer.SetSlice(&m_ObjectConstants);
        }

        // Init MeshGeometry
        {
            const std::array<DirectX::XMFLOAT3, 4> vertices =
            {
                DirectX::XMFLOAT3{ -0.5f, 0.0f,  0.5f },
                DirectX::XMFLOAT3{  0.5f, 0.0f,  0.5f },
                DirectX::XMFLOAT3{ -0.5f, 0.0f, -0.5f },
                DirectX::XMFLOAT3{  0.5f, 0.0f, -0.5f }
            };

            const std::array<std::int16_t, 4> indices{ 0, 1, 2, 3 };

            m_MeshGeometry.InitStaticVertexBuffer(vertices.data(), vertices.size(), uploadBuffer);
            m_MeshGeometry.InitStaticIndexBuffer(indices.data(), indices.size(), uploadBuffer);

            m_MeshGeometry.m_PrimitiveTopology = spieler::renderer::PrimitiveTopology::ControlPointPatchlist4;

            spieler::renderer::SubmeshGeometry& grid{ m_MeshGeometry.CreateSubmesh("grid") };
            grid.IndexCount = indices.size();
            grid.BaseVertexLocation = 0;
            grid.StartIndexLocation = 0;
        }

        // Init RenderItem
        {
            m_RenderItem.MeshGeometry = &m_MeshGeometry;
            m_RenderItem.SubmeshGeometry = &m_MeshGeometry.GetSubmesh("grid");
        }

        // Init RoogSignature
        {
            const spieler::renderer::RootParameter pass
            {
                .Type{ spieler::renderer::RootParameterType::ConstantBufferView },
                .Child{ spieler::renderer::RootDescriptor{ .ShaderRegister{ 0 } } }
            };

            const spieler::renderer::RootParameter object
            {
                .Type{ spieler::renderer::RootParameterType::ConstantBufferView },
                .Child{ spieler::renderer::RootDescriptor{ .ShaderRegister{ 1 } } }
            };

            m_RootSignature = spieler::renderer::RootSignature{ device, { pass, object } };
        }

        // Init PipelineState
        {
            const spieler::renderer::Shader* vs{ nullptr };
            const spieler::renderer::Shader* hs{ nullptr };
            const spieler::renderer::Shader* ds{ nullptr };
            const spieler::renderer::Shader* ps{ nullptr };
            
            // Init Shaders
            {
                vs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Vertex, spieler::renderer::ShaderConfig<per::Tessellation>
                {
                    .Filename{ L"assets/shaders/tessellation.hlsl" },
                    .EntryPoint{ "VS_Main" },
                    //.Permutation{ spieler::renderer::ShaderPermutation<per::Tessellation>{} }
                });

                hs = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Hull, spieler::renderer::ShaderConfig<per::Tessellation>
                {
                    .Filename{ L"assets/shaders/tessellation.hlsl" },
                    .EntryPoint{ "HS_Main" },
                    //.Permutation{ spieler::renderer::ShaderPermutation<per::Tessellation>{} }
                });

                ds = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Domain, spieler::renderer::ShaderConfig<per::Tessellation>
                {
                    .Filename{ L"assets/shaders/tessellation.hlsl" },
                    .EntryPoint{ "DS_Main" },
                    //.Permutation{ spieler::renderer::ShaderPermutation<per::Tessellation>{} }
                });

                ps = &m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Pixel, spieler::renderer::ShaderConfig<per::Tessellation>
                {
                    .Filename{ L"assets/shaders/tessellation.hlsl" },
                    .EntryPoint{ "PS_Main" },
                    //.Permutation{ spieler::renderer::ShaderPermutation<per::Tessellation>{} }
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

            m_PipelineState = spieler::renderer::GraphicsPipelineState{ device, graphicsPipelineStateConfig };
        }

        SPIELER_ASSERT(context.ExecuteCommandList(true));

        return true;
    }

    void TessellationLayer::OnEvent(spieler::Event& event)
    {
        m_CameraController.OnEvent(event);
    }

    void TessellationLayer::OnUpdate(float dt)
    {
        m_CameraController.OnUpdate(dt);

        // Uplodate Pass ConstantBuffer
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
            m_ObjectConstants.World = DirectX::XMMatrixTranspose(m_RenderItem.Transform.GetMatrix()),

            m_ObjectConstantBuffer.GetSlice(&m_ObjectConstants).Update(&m_ObjectConstants, sizeof(m_ObjectConstants));
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

            context.SetPipelineState(m_PipelineState);
            context.SetGraphicsRootSignature(m_RootSignature);

            m_PassConstantBuffer.GetSlice(&m_PassConstants).Bind(context, 0);
            m_ObjectConstantBuffer.GetSlice(&m_ObjectConstants).Bind(context, 1);

            m_MeshGeometry.GetVertexBuffer().GetView().Bind(context);
            m_MeshGeometry.GetIndexBuffer().GetView().Bind(context);

            context.SetPrimitiveTopology(m_MeshGeometry.GetPrimitiveTopology());

            context.GetNativeCommandList()->DrawIndexedInstanced(
                m_MeshGeometry.GetSubmesh("grid").IndexCount,
                1,
                m_MeshGeometry.GetSubmesh("grid").StartIndexLocation,
                m_MeshGeometry.GetSubmesh("grid").BaseVertexLocation,
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

} // namespace sandbox
