#pragma once

#include <spieler/core/layer.hpp>
#include <spieler/system/window.hpp>

#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/context.hpp>
#include <spieler/renderer/camera.hpp>

namespace sandbox
{

    class TessellationLayer final : public spieler::Layer
    {
    private:
        struct PassConstants
        {
            DirectX::XMMATRIX View{};
            DirectX::XMMATRIX Projection{};
            DirectX::XMMATRIX ViewProjection{};
            DirectX::XMFLOAT3 CameraPosition{};
        };

        struct ObjectConstants
        {
            DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
        };

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;

    private:
        void InitViewport();
        void InitScissorRect();
        void InitDepthStencil();

        bool OnWindowResized(spieler::WindowResizedEvent& event);

    private:
        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        std::unordered_map<std::string, spieler::renderer::PipelineState> m_PipelineStates;
        std::unordered_map<std::string, spieler::renderer::RenderItem> m_RenderItems;

        PassConstants m_PassConstants;
        std::shared_ptr<spieler::renderer::BufferResource> m_PassConstantBuffer;

        std::shared_ptr<spieler::renderer::BufferResource> m_ObjectConstantBuffer;

        spieler::renderer::GraphicsFormat m_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::renderer::Texture m_DepthStencil;

        spieler::renderer::Camera m_Camera;
        spieler::renderer::CameraController m_CameraController{ m_Camera };
    };

} // namespace sandbox
