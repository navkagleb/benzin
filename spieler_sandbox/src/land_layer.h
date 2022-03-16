#pragma once

#include <vector>
#include <unordered_map>

#include <spieler/layer.h>
#include <spieler/window/window.h>
#include <spieler/window/window_event.h>
#include <spieler/renderer/renderer.h>
#include <spieler/renderer/descriptor_heap.h>
#include <spieler/renderer/shader.h>
#include <spieler/renderer/pipeline_state.h>
#include <spieler/renderer/mesh.h>

#include "projection_camera_controller.h"

namespace Sandbox
{

    using PipelineStateContainer = std::unordered_map<std::string, Spieler::PipelineState>;

    class LandLayer : public Spieler::Layer
    {
    public:
        LandLayer(Spieler::Window& window, Spieler::Renderer& renderer);

    public:
        bool OnAttach() override;

        void OnEvent(Spieler::Event& event) override;
        void OnUpdate(float dt) override;
        bool OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        bool InitDescriptorHeaps();
        bool InitMeshGeometry();
        bool InitLandGeometry();
        bool InitRootSignature();
        bool InitPipelineState();

        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(Spieler::WindowResizedEvent& event);

    private:
        Spieler::Window&                    m_Window;
        Spieler::Renderer&                  m_Renderer;

        Spieler::DescriptorHeap             m_CBVDescriptorHeap;

        Spieler::Viewport                   m_Viewport;
        Spieler::ScissorRect                m_ScissorRect;

        ProjectionCameraController          m_CameraController;

        Spieler::VertexShader               m_VertexShader;
        Spieler::PixelShader                m_PixelShader;
        Spieler::RootSignature              m_RootSignature;
        PipelineStateContainer              m_PipelineStates;

        DirectX::XMFLOAT4                   m_ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
        bool                                m_IsSolidRasterizerState{ true };
        bool                                m_IsShowDemoWindow{ false };

        // Mesh
        Spieler::MeshGeometry               m_MeshGeometry;
        Spieler::MeshGeometry               m_LandMeshGeometry;

        std::vector<Spieler::RenderItem>    m_RenderItems;
        Spieler::ConstantBuffer             m_PassConstantBuffer;
    };

} // namespace Sandbox