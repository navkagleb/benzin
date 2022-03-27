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

#include "projection_camera_controller.hpp"

namespace Sandbox
{
    template <typename T>
    using LookUpTable = std::unordered_map<std::string, T>;

    struct PassConstants
    {
        DirectX::XMMATRIX View{};
        DirectX::XMMATRIX Projection{};
    };

    struct ObjectConstants
    {
        DirectX::XMMATRIX World{};
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
        bool InitTextures();
        bool InitMeshGeometries();
        bool InitRootSignatures();
        bool InitPipelineStates();
        void InitConstantBuffers();

        void UpdateViewport();
        void UpdateScissorRect();

    private:
        Spieler::Window& m_Window;
        Spieler::Renderer& m_Renderer;

        Spieler::Viewport m_Viewport;
        Spieler::ScissorRect m_ScissorRect;

        ProjectionCameraController m_CameraController;

        LookUpTable<Spieler::DescriptorHeap> m_DescriptorHeaps;
        LookUpTable<Spieler::UploadBuffer> m_UploadBuffers;
        LookUpTable<Spieler::Texture2D> m_Textures;
        LookUpTable<Spieler::ShaderResourceView> m_SRVs;
        LookUpTable<Spieler::Sampler> m_Samplers;
        LookUpTable<Spieler::MeshGeometry> m_MeshGeometries;
        LookUpTable<Spieler::RootSignature> m_RootSignatures;
        LookUpTable<Spieler::VertexShader> m_VertexShaders;
        LookUpTable<Spieler::PixelShader> m_PixelShaders;
        LookUpTable<Spieler::PipelineState> m_PipelineStates;
        LookUpTable<Spieler::RenderItem> m_RenderItems;
        LookUpTable<Spieler::ConstantBuffer> m_ConstantBuffers;
    };

} // namespace Sandbox