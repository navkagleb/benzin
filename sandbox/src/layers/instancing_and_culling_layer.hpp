#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/renderer/buffer.hpp>
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/context.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/pipeline_state.hpp>

#include "projection_camera_controller.hpp"

namespace sandbox
{

    class InstancingAndCullingLayer final : public spieler::Layer
    {
    private:
        struct RenderItem
        {
            struct Instance
            {
                spieler::renderer::Transform Transform;
                uint32_t MaterialIndex{ 0 };
            };

            const spieler::renderer::MeshGeometry* MeshGeometry{ nullptr };
            const spieler::renderer::SubmeshGeometry* SubmeshGeometry{ nullptr };
            spieler::renderer::PrimitiveTopology PrimitiveTopology{ spieler::renderer::PrimitiveTopology::Unknown };

            std::vector<Instance> Instances;
            uint32_t VisibleInstanceCount{ 0 };
        };

        struct MaterialData
        {
            DirectX::XMFLOAT4 DiffuseAlbedo{ 1.0f, 1.0f, 1.0f, 1.0f };
            DirectX::XMFLOAT3 FresnelR0{ 0.01f, 0.01f, 0.01f };
            float Roughness{ 0.25f };
            DirectX::XMMATRIX Transform{ DirectX::XMMatrixIdentity() };
            uint32_t DiffuseMapIndex{ 0 };
            uint32_t StructuredBufferIndex{ 0 };
        };

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

        void OnWindowResized();

    private:
        static const spieler::renderer::GraphicsFormat ms_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        std::unordered_map<std::string, spieler::renderer::Buffer> m_Buffers;
        std::unordered_map<std::string, spieler::renderer::Texture> m_Textures;

        spieler::renderer::MeshGeometry m_MeshGeometry;
        spieler::renderer::RootSignature m_RootSignature;
        spieler::renderer::ShaderLibrary m_ShaderLibrary;
        spieler::renderer::PipelineState m_PipelineState;
        
        std::unordered_map<std::string, RenderItem> m_RenderItems;
        std::unordered_map<std::string, MaterialData> m_Materials;

        Camera m_Camera;
        CameraController m_CameraController{ m_Camera };

        bool m_IsCullingEnabled{ true };
    };

} // namespace sandbox
