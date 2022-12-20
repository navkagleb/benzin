#pragma once

#include <spieler/math/math.hpp>

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
#include <spieler/engine/mesh.hpp>
#include <spieler/engine/camera.hpp>
#include <spieler/engine/light.hpp>

#include "passes/blur_pass.hpp"
#include "techniques/picking_technique.hpp"

namespace sandbox
{

    class DynamicCubeMap
    {
    public:
        DynamicCubeMap(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        spieler::Texture& GetCubeMap() { return m_CubeMap; }
        spieler::Texture& GetDepthStencil() { return m_DepthStencil; }

        const DirectX::XMVECTOR& GetPosition() const { return m_Position; }
        void SetPosition(const DirectX::XMVECTOR& position);

        const spieler::Camera& GetCamera(uint32_t index) const { return m_Cameras[index]; }

        const spieler::Viewport& GetViewport() const { return m_Viewport; }
        const spieler::ScissorRect& GetScissorRect() const { return m_ScissorRect; }

    public:
        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitCubeMap(spieler::Device& device, uint32_t width, uint32_t height);
        void InitDepthStencil(spieler::Device& device, uint32_t width, uint32_t height);
        void InitCameras(uint32_t width, uint32_t height);
        void InitViewport(float width, float height);

    private:
        spieler::Texture m_CubeMap;
        spieler::Texture m_DepthStencil;

        DirectX::XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f, 1.0f };
        std::array<spieler::Camera, 6> m_Cameras;

        spieler::Viewport m_Viewport;
        spieler::ScissorRect m_ScissorRect;
    };

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

        void OnWindowResized();

        void RenderEntities(const spieler::PipelineState& pso, const std::span<const spieler::Entity*>& entities);
        void RenderFullscreenQuad(const spieler::TextureShaderResourceView& srv);

    private:
        static const spieler::GraphicsFormat ms_DepthStencilFormat{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        spieler::Window& m_Window;
        spieler::Device& m_Device;
        spieler::CommandQueue& m_CommandQueue;
        spieler::SwapChain& m_SwapChain;

        spieler::GraphicsCommandList m_GraphicsCommandList;
        spieler::GraphicsCommandList m_PostEffectsGraphicsCommandList;

        std::unordered_map<std::string, spieler::Buffer> m_Buffers;
        std::unordered_map<std::string, spieler::Texture> m_Textures;

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

        spieler::Camera m_Camera;
        spieler::CameraController m_CameraController{ m_Camera };

        PointLightController m_PointLightController;

        std::unique_ptr<DynamicCubeMap> m_DynamicCubeMap;

        bool m_IsCullingEnabled{ true };

        std::unique_ptr<BlurPass> m_BlurPass;
        std::unique_ptr<PickingTechnique> m_PickingTechnique;
    };

} // namespace sandbox
