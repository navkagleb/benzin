#pragma once

#include <spieler/core/layer.hpp>
#include <spieler/system/window.hpp>

#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/constant_buffer.hpp>
#include <spieler/renderer/texture.hpp>

#include "projection_camera_controller.hpp"

namespace sandbox
{

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

    class TessellationLayer : public spieler::Layer
    {
    public:
        TessellationLayer(spieler::Window& window);

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void InitViewport();
        void InitScissorRect();
        void InitDepthStencil();

        bool OnWindowResized(spieler::WindowResizedEvent& event);

    private:
        spieler::Window& m_Window;

        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        std::unordered_map<std::string, spieler::renderer::PipelineState> m_PipelineStates;
        std::unordered_map<std::string, spieler::renderer::RenderItem> m_RenderItems;

        PassConstants m_PassConstants;
        spieler::renderer::ConstantBuffer m_PassConstantBuffer;

        std::unordered_map<std::string, ObjectConstants> m_ObjectConstants;
        spieler::renderer::ConstantBuffer m_ObjectConstantBuffer;

        spieler::renderer::GraphicsFormat m_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::renderer::Texture2D m_DepthStencil;

        ProjectionCameraController m_CameraController;
    };

} // namespace sandbox
