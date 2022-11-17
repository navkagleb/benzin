#pragma once

#include <vector>
#include <unordered_map>

#if 0

#include <spieler/layer.hpp>
#include <spieler/window/window.hpp>
#include <spieler/window/window_event.hpp>
#include <spieler/graphics/renderer.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/mesh.hpp>
#include <spieler/graphics/light.hpp>
#include <spieler/graphics/upload_buffer.hpp>
#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>

#include "projection_camera_controller.hpp"
#include "waves.hpp"
#include "light_controller.hpp"

namespace sandbox
{

    template <typename T>
    using LookUpTable = std::unordered_map<std::string, T>;

    struct ColorVertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT4 Color{};
    };

    struct Vertex
    {
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT3 Normal{};
        DirectX::XMFLOAT2 TexCoord{};
    };

    struct MeshUploadBuffer
    {
        spieler::UploadBuffer VertexUploadBuffer;
        spieler::UploadBuffer IndexUploadBuffer;
    };

    class LandLayer : public spieler::Layer
    {
    public:
        LandLayer(spieler::Window& window, spieler::Renderer& renderer);

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        bool OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        bool InitDescriptorHeaps();
        bool InitUploadBuffers();
        bool InitTextures(spieler::UploadBuffer& uploadBuffer);
        void InitMaterials();
        bool InitMeshGeometry(spieler::UploadBuffer& uploadBuffer);
        bool InitLandGeometry(spieler::UploadBuffer& uploadBuffer);
        bool InitWavesGeometry(spieler::UploadBuffer& uploadBuffer);
        bool InitRootSignature();
        bool InitPipelineState();

        void InitPassConstantBuffer();
        void InitLightControllers();
        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(spieler::WindowResizedEvent& event);

        void UpdateWavesVertexBuffer(float dt);

        bool RenderLitRenderItems();
        bool RenderBlendedRenderItems();
        bool RenderColorRenderItems();

    private:
        const std::uint32_t                 m_RenderItemCount{ 7 };
        const std::uint32_t                 m_MaterialCount{ 2 };

    private:
        spieler::Window&                    m_Window;
        spieler::Renderer&                  m_Renderer;

        spieler::Viewport                   m_Viewport;
        spieler::ScissorRect                m_ScissorRect;

        DirectX::XMFLOAT4                   m_ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };

        ProjectionCameraController          m_CameraController;

        LookUpTable<spieler::DescriptorHeap> m_DescriptorHeaps;
        LookUpTable<spieler::Reference<spieler::UploadBuffer>> m_UploadBuffers;
        LookUpTable<spieler::Texture2D>     m_Textures;
        LookUpTable<spieler::MeshGeometry>  m_MeshGeometries;
        LookUpTable<spieler::RootSignature> m_RootSignatures;
        LookUpTable<spieler::VertexShader>  m_VertexShaders;
        LookUpTable<spieler::PixelShader>   m_PixelShaders;
        LookUpTable<spieler::PipelineState> m_PipelineStates;
        LookUpTable<spieler::Material>      m_Materials;

        spieler::VertexBufferView           m_WaterVertexBufferView;
        
        LookUpTable<spieler::RenderItem>    m_LitRenderItems;
        LookUpTable<spieler::RenderItem>    m_BlendedRenderItems;
        LookUpTable<spieler::RenderItem>    m_ColorRenderItems;
        spieler::ConstantBuffer             m_PassConstantBuffer;

        DirectionalLightController          m_DirectionalLightController;
        PointLightController                m_PointLightController;
        SpotLightController                 m_SpotLightController;

        Waves                               m_Waves;
    };

} // namespace sandbox

#endif