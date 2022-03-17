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
#include "waves.h"

namespace Sandbox
{

    using PipelineStateContainer    = std::unordered_map<std::string, Spieler::PipelineState>;
    using MeshGeometryContainer     = std::unordered_map<std::string, Spieler::MeshGeometry>;

    struct ColorVertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT4 Color{};
    };

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
        bool InitUploadBuffers();
        bool InitMeshGeometry();
        bool InitLandGeometry();
        bool InitWavesGeometry();
        bool InitRootSignature();
        bool InitPipelineState();

        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(Spieler::WindowResizedEvent& event);

    private:
        const std::uint32_t                 m_RenderItemCount{ 2 };

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

        Spieler::UploadBuffer               m_PassUploadBuffer;
        Spieler::UploadBuffer               m_ObjectUploadBuffer;
        Spieler::UploadBuffer               m_WavesUploadBuffer;

        MeshGeometryContainer               m_MeshGeometries;

        std::vector<Spieler::RenderItem>    m_RenderItems;
        Spieler::ConstantBuffer             m_PassConstantBuffer;

        Waves                               m_Waves;
    };

} // namespace Sandbox