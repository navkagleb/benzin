#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/context.hpp>
#include <spieler/renderer/constant_buffer.hpp>

#include "projection_camera_controller.hpp"

namespace sandbox
{

    class DynamicIndexingLayer final : public spieler::Layer
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
            uint32_t MaterialIndex{ 0 };
        };

        struct MaterialConstants
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
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
        ProjectionCameraController m_CameraController;

        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        const spieler::renderer::GraphicsFormat m_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::renderer::Texture2D m_DepthStencil;

        std::unordered_map<std::string, spieler::renderer::Texture2D> m_Textures;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        spieler::renderer::PipelineState m_PipelineState;

        std::unordered_map<std::string, std::unique_ptr<spieler::renderer::RenderItem>> m_RenderItems;

        PassConstants m_PassConstants;
        spieler::renderer::ConstantBuffer m_PassConstantBuffer;

        spieler::renderer::ConstantBuffer m_ObjectConstantBuffer;

        std::unordered_map<std::string, MaterialConstants> m_MaterialConstants;
        spieler::renderer::ConstantBuffer m_MaterialConstantBuffer;
    };

} // namespace sandbox
