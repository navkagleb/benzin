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
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/root_signature.hpp>

#include "projection_camera_controller.hpp"
#include "waves.hpp"
#include "light_controller.hpp"

namespace Sandbox
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
        bool InitDescriptorHeaps();
        bool InitUploadBuffers();
        bool InitTextures(Spieler::UploadBuffer& grassTextureUploadBuffer, Spieler::UploadBuffer& waterTextureUploadBuffer);
        void InitMaterials();
        bool InitMeshGeometry(MeshUploadBuffer& meshUploadBuffer);
        bool InitLandGeometry(MeshUploadBuffer& landUploadBuffer);
        bool InitWavesGeometry(MeshUploadBuffer& wavesUploadBuffer);
        bool InitRootSignature();
        bool InitPipelineState();

        void InitPassConstantBuffer();
        void InitLightControllers();
        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(Spieler::WindowResizedEvent& event);

        void UpdateWavesVertexBuffer(float dt);

        bool RenderLitRenderItems();
        bool RenderBlendedRenderItems();
        bool RenderColorRenderItems();

    private:
        const std::uint32_t                 m_RenderItemCount{ 7 };
        const std::uint32_t                 m_MaterialCount{ 2 };

    private:
        Spieler::Window&                    m_Window;
        Spieler::Renderer&                  m_Renderer;

        Spieler::Viewport                   m_Viewport;
        Spieler::ScissorRect                m_ScissorRect;

        DirectX::XMFLOAT4                   m_ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };

        ProjectionCameraController          m_CameraController;

        LookUpTable<Spieler::DescriptorHeap> m_DescriptorHeaps;
        LookUpTable<Spieler::UploadBuffer>  m_UploadBuffers;
        LookUpTable<Spieler::Texture2D>     m_Textures;
        LookUpTable<Spieler::VertexBufferView> m_VertexBufferViews;
        LookUpTable<Spieler::IndexBufferView> m_IndexBufferViews;
        LookUpTable<Spieler::MeshGeometry>  m_MeshGeometries;
        LookUpTable<Spieler::RootSignature> m_RootSignatures;
        LookUpTable<Spieler::VertexShader>  m_VertexShaders;
        LookUpTable<Spieler::PixelShader>   m_PixelShaders;
        LookUpTable<Spieler::PipelineState> m_PipelineStates;
        LookUpTable<Spieler::Material>      m_Materials;
        
        LookUpTable<Spieler::RenderItem>    m_LitRenderItems;
        LookUpTable<Spieler::RenderItem>    m_BlendedRenderItems;
        LookUpTable<Spieler::RenderItem>    m_ColorRenderItems;
        Spieler::ConstantBuffer             m_PassConstantBuffer;

        DirectionalLightController          m_DirectionalLightController;
        PointLightController                m_PointLightController;
        SpotLightController                 m_SpotLightController;

        Waves                               m_Waves;
    };

} // namespace Sandbox