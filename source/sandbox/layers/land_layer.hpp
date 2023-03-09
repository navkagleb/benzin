#pragma once

#include <vector>
#include <unordered_map>

#if 0

#include <benzin/layer.hpp>
#include <benzin/window/window.hpp>
#include <benzin/window/window_event.hpp>
#include <benzin/graphics/renderer.hpp>
#include <benzin/graphics/shader.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/mesh.hpp>
#include <benzin/graphics/light.hpp>
#include <benzin/graphics/upload_buffer.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/root_signature.hpp>

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
        benzin::UploadBuffer VertexUploadBuffer;
        benzin::UploadBuffer IndexUploadBuffer;
    };

    class LandLayer : public benzin::Layer
    {
    public:
        LandLayer(benzin::Window& window, benzin::Renderer& renderer);

    public:
        bool OnAttach() override;

        void OnEvent(benzin::Event& event) override;
        void OnUpdate(float dt) override;
        bool OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        bool InitDescriptorHeaps();
        bool InitUploadBuffers();
        bool InitTextures(benzin::UploadBuffer& uploadBuffer);
        void InitMaterials();
        bool InitMeshGeometry(benzin::UploadBuffer& uploadBuffer);
        bool InitLandGeometry(benzin::UploadBuffer& uploadBuffer);
        bool InitWavesGeometry(benzin::UploadBuffer& uploadBuffer);
        bool InitRootSignature();
        bool InitPipelineState();

        void InitPassConstantBuffer();
        void InitLightControllers();
        void InitViewport();
        void InitScissorRect();

        bool OnWindowResized(benzin::WindowResizedEvent& event);

        void UpdateWavesVertexBuffer(float dt);

        bool RenderLitRenderItems();
        bool RenderBlendedRenderItems();
        bool RenderColorRenderItems();

    private:
        const std::uint32_t                 m_RenderItemCount{ 7 };
        const std::uint32_t                 m_MaterialCount{ 2 };

    private:
        benzin::Window&                    m_Window;
        benzin::Renderer&                  m_Renderer;

        benzin::Viewport                   m_Viewport;
        benzin::ScissorRect                m_ScissorRect;

        DirectX::XMFLOAT4                   m_ClearColor{ 0.1f, 0.1f, 0.1f, 1.0f };

        ProjectionCameraController          m_CameraController;

        LookUpTable<benzin::DescriptorHeap> m_DescriptorHeaps;
        LookUpTable<benzin::Reference<benzin::UploadBuffer>> m_UploadBuffers;
        LookUpTable<benzin::Texture2D>     m_Textures;
        LookUpTable<benzin::MeshGeometry>  m_MeshGeometries;
        LookUpTable<benzin::RootSignature> m_RootSignatures;
        LookUpTable<benzin::VertexShader>  m_VertexShaders;
        LookUpTable<benzin::PixelShader>   m_PixelShaders;
        LookUpTable<benzin::PipelineState> m_PipelineStates;
        LookUpTable<benzin::Material>      m_Materials;

        benzin::VertexBufferView           m_WaterVertexBufferView;
        
        LookUpTable<benzin::RenderItem>    m_LitRenderItems;
        LookUpTable<benzin::RenderItem>    m_BlendedRenderItems;
        LookUpTable<benzin::RenderItem>    m_ColorRenderItems;
        benzin::ConstantBuffer             m_PassConstantBuffer;

        DirectionalLightController          m_DirectionalLightController;
        PointLightController                m_PointLightController;
        SpotLightController                 m_SpotLightController;

        Waves                               m_Waves;
    };

} // namespace sandbox

#endif