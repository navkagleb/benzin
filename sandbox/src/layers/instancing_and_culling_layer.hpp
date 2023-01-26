#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/system/window.hpp>
#include <spieler/graphics/buffer.hpp>
#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/graphics_command_list.hpp>
#include <spieler/graphics/command_queue.hpp>
#include <spieler/graphics/swap_chain.hpp>
#include <spieler/graphics/render_target_cube_map.hpp>
#include <spieler/engine/mesh.hpp>
#include <spieler/engine/camera.hpp>
#include <spieler/engine/light.hpp>

#include "passes/blur_pass.hpp"
#include "techniques/picking_technique.hpp"

namespace sandbox
{

    class InstancingAndCullingLayer final : public spieler::Layer
    {
    public:
        class PointLightController
        {
        public:
            PointLightController() = default;
            PointLightController(spieler::Light* light);

        public:
            void OnImGuiRender();

        private:
            spieler::Light* m_Light{ nullptr };
        };

    public:
        InstancingAndCullingLayer(spieler::Window& window, spieler::Device& device, spieler::CommandQueue& commandQueue, spieler::SwapChain& swapChain);

    public:
        bool OnAttach() override;
        bool OnDetach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void InitTextures();
        void InitMesh();

        void InitBuffers();

        void InitRootSignature();
        void InitPipelineState();

        void InitRenderTarget();
        void InitDepthStencil();

        void InitMaterials();
        void InitEntities();

        void OnWindowResized();

        void RenderEntities(const spieler::PipelineState& pso, const std::span<const spieler::Entity*>& entities);
        void RenderFullscreenQuad(const spieler::Descriptor& srv);

        bool OnWindowResized(spieler::WindowResizedEvent& event);
        bool OnMouseButtonPressed(spieler::MouseButtonPressedEvent& event);

    private:
        static const spieler::GraphicsFormat ms_DepthStencilFormat{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        spieler::Window& m_Window;
        spieler::Device& m_Device;
        spieler::CommandQueue& m_CommandQueue;
        spieler::SwapChain& m_SwapChain;

        spieler::GraphicsCommandList m_GraphicsCommandList;
        spieler::GraphicsCommandList m_PostEffectsGraphicsCommandList;

        std::unordered_map<std::string, std::shared_ptr<spieler::BufferResource>> m_Buffers;
        std::unordered_map<std::string, std::shared_ptr<spieler::TextureResource>> m_Textures;

        spieler::Mesh m_Mesh;
        spieler::RootSignature m_RootSignature;
        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;
        std::unordered_map<std::string, spieler::PipelineState> m_PipelineStates;
        
        std::unordered_map<std::string, std::unique_ptr<spieler::Entity>> m_Entities;
        std::vector<const spieler::Entity*> m_DefaultEntities;
        std::vector<const spieler::Entity*> m_LightSourceEntities;
        std::vector<const spieler::Entity*> m_PickableEntities;

        std::unordered_map<std::string, spieler::Material> m_Materials;

        const DirectX::XMFLOAT4 m_AmbientLight{ 0.1f, 0.1f, 0.1f, 1.0f };

        spieler::PerspectiveProjection m_PerspectiveProjection{ DirectX::XMConvertToRadians(60.0f), m_Window.GetAspectRatio(), 0.1f, 1000.0f };
        spieler::Camera m_Camera{ &m_PerspectiveProjection };
        spieler::FlyCameraController m_FlyCameraController{ &m_Camera };

        PointLightController m_PointLightController;

        spieler::RenderTargetCubeMap m_RenderTargetCubeMap;

        bool m_IsCullingEnabled{ true };

        std::unique_ptr<BlurPass> m_BlurPass;
        std::unique_ptr<PickingTechnique> m_PickingTechnique;
    };

} // namespace sandbox
