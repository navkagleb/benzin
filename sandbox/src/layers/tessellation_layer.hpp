#pragma once

#include <spieler/core/layer.hpp>
#include <spieler/system/window.hpp>

#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/graphics_command_list.hpp>

#include <spieler/engine/camera.hpp>
#include <spieler/engine/mesh.hpp>

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
        spieler::Viewport m_Viewport;
        spieler::ScissorRect m_ScissorRect;

        spieler::MeshGeometry m_MeshGeometry;
        spieler::RootSignature m_RootSignature;
        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;
        std::unordered_map<std::string, spieler::PipelineState> m_PipelineStates;
        std::unordered_map<std::string, spieler::RenderItem> m_RenderItems;

        PassConstants m_PassConstants;
        std::shared_ptr<spieler::BufferResource> m_PassConstantBuffer;

        std::shared_ptr<spieler::BufferResource> m_ObjectConstantBuffer;

        spieler::GraphicsFormat m_DepthStencilFormat{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::Texture m_DepthStencil;

        spieler::Camera m_Camera;
        spieler::CameraController m_CameraController{ m_Camera };
    };

} // namespace sandbox
