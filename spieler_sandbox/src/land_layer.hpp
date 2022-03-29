#pragma once

#include <vector>
#include <unordered_map>

#include <spieler/layer.hpp>
#include <spieler/window/window.hpp>
#include <spieler/window/window_event.hpp>
#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/descriptor_heap.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/light.hpp>
#include <spieler/renderer/upload_buffer.hpp>

#include "projection_camera_controller.hpp"
#include "waves.hpp"
#include "light_controller.hpp"

namespace Sandbox
{

    using PipelineStateContainer = std::unordered_map<std::string, Spieler::PipelineState>;
    using MeshGeometryContainer = std::unordered_map<std::string, Spieler::MeshGeometry>;
    using MaterialContainer = std::unordered_map<std::string, Spieler::Material>;
    using VertexShaderContainer = std::unordered_map<std::string, Spieler::VertexShader>;
    using PixelShaderContainer = std::unordered_map<std::string, Spieler::PixelShader>;
    using VertexBufferViewContainer = std::unordered_map<std::string, Spieler::VertexBufferView>;
    using IndexBufferViewContainer = std::unordered_map<std::string, Spieler::IndexBufferView>;

    struct ColorVertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT4 Color{};
    };

    struct NormalVertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT3 Normal{};
    };

    struct MeshUploadBuffer
    {
        Spieler::UploadBuffer VertexUploadBuffer;
        Spieler::UploadBuffer IndexUploadBuffer;
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
        bool InitUploadBuffers();
        bool InitMeshGeometry(MeshUploadBuffer& meshUploadBuffer);
        bool InitLandGeometry(MeshUploadBuffer& landUploadBuffer);
        bool InitWavesGeometry(MeshUploadBuffer& wavesUploadBuffer);
        bool InitRootSignature();
        bool InitPipelineState();

        void InitMaterials();
        void InitPassConstantBuffer();
        void InitLightControllers();
        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(Spieler::WindowResizedEvent& event);

        void UpdateWavesVertexBuffer();

        bool RenderLitRenderItems();
        bool RenderColorRenderItems();

    private:
        const std::uint32_t                 m_RenderItemCount{ 7 };
        const std::uint32_t                 m_MaterialCount{ 2 };

    private:
        Spieler::Window&                    m_Window;
        Spieler::Renderer&                  m_Renderer;

        Spieler::Viewport                   m_Viewport;
        Spieler::ScissorRect                m_ScissorRect;

        ProjectionCameraController          m_CameraController;

        VertexShaderContainer               m_VertexShaders;
        PixelShaderContainer                m_PixelShaders;
        Spieler::RootSignature              m_RootSignature;
        PipelineStateContainer              m_PipelineStates;

        DirectX::XMFLOAT4                   m_ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };
    
        Spieler::UploadBuffer               m_PassUploadBuffer;
        Spieler::UploadBuffer               m_ObjectUploadBuffer;
        Spieler::UploadBuffer               m_WavesUploadBuffer;
        Spieler::UploadBuffer               m_MaterialUploadBuffer;

        MeshGeometryContainer               m_MeshGeometries;
        MaterialContainer                   m_Materials;

        VertexBufferViewContainer           m_VertexBufferViews;
        IndexBufferViewContainer            m_IndexBufferViews;

        std::vector<Spieler::RenderItem>    m_LitRenderItems;
        std::vector<Spieler::RenderItem>    m_ColorRenderItems;
        Spieler::ConstantBuffer             m_PassConstantBuffer;

        DirectionalLightController          m_DirectionalLightController;
        PointLightController                m_PointLightController;
        SpotLightController                 m_SpotLightController;

        Waves                               m_Waves;
    };

} // namespace Sandbox