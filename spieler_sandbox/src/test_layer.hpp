#pragma once

#include <unordered_map>
#include <string>

#include <spieler/layer.hpp>
#include <spieler/window/window.hpp>
#include <spieler/renderer/renderer.hpp>
#include <spieler/renderer/descriptor_heap.hpp>
#include <spieler/renderer/upload_buffer.hpp>
#include <spieler/renderer/mesh.hpp>
#include <spieler/renderer/shader.hpp>
#include <spieler/renderer/root_signature.hpp>
#include <spieler/renderer/pipeline_state.hpp>
#include <spieler/renderer/texture.hpp>
#include <spieler/renderer/sampler.hpp>
#include <spieler/renderer/light.hpp>

#include "projection_camera_controller.hpp"
#include "light_controller.hpp"

namespace Sandbox
{
    template <typename T>
    using LookUpTable = std::unordered_map<std::string, T>;

    constexpr std::uint32_t MAX_LIGHT_COUNT{ 16 };

    struct PassConstants
    {
        alignas(16) DirectX::XMMATRIX View{};
        alignas(16) DirectX::XMMATRIX Projection{};
        alignas(16) DirectX::XMFLOAT3 CameraPosition{};
        alignas(16) DirectX::XMFLOAT4 AmbientLight{};
        alignas(16) Spieler::LightConstants Lights[MAX_LIGHT_COUNT];
    };

    struct LitObjectConstants
    {
        DirectX::XMMATRIX World{ DirectX::XMMatrixIdentity() };
        DirectX::XMMATRIX TextureTransform{ DirectX::XMMatrixIdentity() };
    };

    struct UnlitObjectConstants
    {
        DirectX::XMMATRIX World{};
        DirectX::XMFLOAT4 Color{};
    };

    class TestLayer : public Spieler::Layer
    {
    public:
        TestLayer(Spieler::Window& window, Spieler::Renderer& renderer);

    public:
        bool OnAttach() override;

        void OnEvent(Spieler::Event& event) override;
        void OnUpdate(float dt) override;
        bool OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        bool InitDescriptorHeaps();
        bool InitUploadBuffers();
        bool InitTextures(Spieler::UploadBuffer& woodCrateTextureUploadBuffer, Spieler::UploadBuffer& wireFenceTextureUploadBuffer);
        void InitMaterials();
        bool InitMeshGeometries(Spieler::UploadBuffer& vertexUploadBuffer, Spieler::UploadBuffer& indexUploadBuffer);
        bool InitRootSignatures();
        bool InitPipelineStates();
        void InitConstantBuffers();
        void InitLights();

        void UpdateViewport();
        void UpdateScissorRect();

        bool OnWindowResized(Spieler::WindowResizedEvent& event);

    private:
        Spieler::Window& m_Window;
        Spieler::Renderer& m_Renderer;

        Spieler::Viewport m_Viewport;
        Spieler::ScissorRect m_ScissorRect;

        ProjectionCameraController m_CameraController;

        LookUpTable<Spieler::VertexBufferView> m_VertexBufferViews;
        LookUpTable<Spieler::IndexBufferView> m_IndexBufferViews;

        LookUpTable<Spieler::DescriptorHeap> m_DescriptorHeaps;
        LookUpTable<Spieler::UploadBuffer> m_UploadBuffers;
        LookUpTable<Spieler::Texture2D> m_Textures;
        LookUpTable<Spieler::Sampler> m_Samplers;
        LookUpTable<Spieler::Material> m_Materials;
        LookUpTable<Spieler::MeshGeometry> m_MeshGeometries;
        LookUpTable<Spieler::RootSignature> m_RootSignatures;
        LookUpTable<Spieler::VertexShader> m_VertexShaders;
        LookUpTable<Spieler::PixelShader> m_PixelShaders;
        LookUpTable<Spieler::PipelineState> m_PipelineStates;
        LookUpTable<Spieler::RenderItem> m_LitRenderItems;
        LookUpTable<Spieler::RenderItem> m_UnlitRenderItems;
        LookUpTable<Spieler::ConstantBuffer> m_ConstantBuffers;
        DirectionalLightController m_DirectionalLightController;
    };

} // namespace Sandbox