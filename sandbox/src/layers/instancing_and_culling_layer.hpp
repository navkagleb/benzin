#pragma once

#include <benzin/core/layer.hpp>

#include <benzin/system/window.hpp>
#include <benzin/graphics/api/buffer.hpp>
#include <benzin/graphics/api/texture.hpp>
#include <benzin/graphics/api/root_signature.hpp>
#include <benzin/graphics/api/shader.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/api/graphics_command_list.hpp>
#include <benzin/graphics/api/command_queue.hpp>
#include <benzin/graphics/api/swap_chain.hpp>
#include <benzin/graphics/render_target_cube_map.hpp>
#include <benzin/graphics/shadow_map.hpp>
#include <benzin/engine/mesh.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/light.hpp>

#include "passes/blur_pass.hpp"
#include "techniques/picking_technique.hpp"

namespace sandbox
{

    class InstancingAndCullingLayer final : public benzin::Layer
    {
    private:
        struct FrameContext
        {
            std::shared_ptr<benzin::BufferResource> PassConstantBuffer;
            std::shared_ptr<benzin::BufferResource> EntityStructuredBuffer;
            std::shared_ptr<benzin::BufferResource> MaterialStructuredBuffer;
        };

    public:
        InstancingAndCullingLayer(benzin::Window& window, benzin::Device& device, benzin::CommandQueue& commandQueue, benzin::SwapChain& swapChain);

    public:
        bool OnAttach() override;
        bool OnDetach() override;

        void OnEvent(benzin::Event& event) override;
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

        void RenderEntities(const benzin::PipelineState& pso, const std::span<const benzin::Entity*>& entities);
        void RenderFullscreenQuad(const benzin::Descriptor& srv);

        bool OnWindowResized(benzin::WindowResizedEvent& event);
        bool OnMouseButtonPressed(benzin::MouseButtonPressedEvent& event);

    private:
        static const benzin::GraphicsFormat ms_DepthStencilFormat{ benzin::GraphicsFormat::D24UnsignedNormS8UnsignedInt };

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::CommandQueue& m_CommandQueue;
        benzin::SwapChain& m_SwapChain;

        std::array<FrameContext, benzin::config::GetBackBufferCount()> m_FrameContexts;

        std::unordered_map<std::string, std::shared_ptr<benzin::TextureResource>> m_Textures;

        benzin::Mesh m_Mesh;
        std::unique_ptr<benzin::RootSignature> m_RootSignature;
        std::unordered_map<std::string, std::shared_ptr<benzin::Shader>> m_ShaderLibrary;
        std::unordered_map<std::string, std::unique_ptr<benzin::PipelineState>> m_PipelineStates;
        
        std::unordered_map<std::string, std::unique_ptr<benzin::Entity>> m_Entities;
        std::vector<const benzin::Entity*> m_DefaultEntities;
        std::vector<const benzin::Entity*> m_LightSourceEntities;
        std::vector<const benzin::Entity*> m_PickableEntities;

        benzin::Light* m_DirectionalLight{ nullptr };

        std::unordered_map<std::string, benzin::Material> m_Materials;

        const DirectX::XMFLOAT4 m_AmbientLight{ 0.1f, 0.1f, 0.1f, 1.0f };

        benzin::PerspectiveProjection m_PerspectiveProjection{ DirectX::XMConvertToRadians(60.0f), m_Window.GetAspectRatio(), 0.1f, 1000.0f };
        benzin::Camera m_Camera{ &m_PerspectiveProjection };
        benzin::FlyCameraController m_FlyCameraController{ &m_Camera };

        benzin::RenderTargetCubeMap m_RenderTargetCubeMap;
        benzin::ShadowMap m_ShadowMap;

        bool m_IsCullingEnabled{ true };

        std::unique_ptr<BlurPass> m_BlurPass;
        std::unique_ptr<PickingTechnique> m_PickingTechnique;
    };

} // namespace sandbox
