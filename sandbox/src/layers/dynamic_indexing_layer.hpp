#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/context.hpp>
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/camera.hpp>

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

        struct Material
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
            uint32_t ConstantBufferIndex{ 0 };
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
        spieler::renderer::Camera m_Camera;
        spieler::renderer::CameraController m_CameraController{ m_Camera };

        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        const spieler::renderer::GraphicsFormat m_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::renderer::Texture m_DepthStencil;

        std::unordered_map<std::string, spieler::renderer::Texture> m_Textures;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        spieler::renderer::PipelineState m_PipelineState;

        std::unordered_map<std::string, std::unique_ptr<spieler::renderer::RenderItem>> m_RenderItems;

        PassConstants m_PassConstants;
        spieler::renderer::BufferResource m_PassConstantBuffer;

        spieler::renderer::BufferResource m_ObjectConstantBuffer;

        std::unordered_map<std::string, Material> m_Materials;
        spieler::renderer::BufferResource m_MaterialConstantBuffer;
    };

} // namespace sandbox
