#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/system/window.hpp>

#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/sampler.hpp>
#include <spieler/graphics/graphics_command_list.hpp>

#include <spieler/engine/mesh.hpp>
#include <spieler/engine/light.hpp>
#include <spieler/engine/camera.hpp>

#include "passes/blur_pass.hpp"
#include "passes/sobel_filter_pass.hpp"

namespace sandbox
{

#if 0
    constexpr uint32_t g_MaxLightCount{ 16 };

    namespace per
    {

        enum class Color {};

        enum class Texture
        {
            DIRECTIONAL_LIGHT_COUNT,
            USE_ALPHA_TEST,
            ENABLE_FOG
        };

        enum class Billboard
        {
            DIRECTIONAL_LIGHT_COUNT,
            USE_ALPHA_TEST,
            ENABLE_FOG
        };

        enum class Composite {};

    } // namespace per

    class TestLayer final : public spieler::Layer
    {
    private:
        struct PassConstants
        {
            struct Fog
            {
                DirectX::XMFLOAT4 Color{ 0.0f, 0.0f, 0.0f, 0.0f };
                float Start{ 0.0f };
                float Range{ 0.0f };
            };

            DirectX::XMMATRIX View{};
            DirectX::XMMATRIX Projection{};
            DirectX::XMFLOAT3 CameraPosition{};
            DirectX::XMFLOAT4 AmbientLight{};
            std::array<spieler::LightConstants, g_MaxLightCount> Lights;

            Fog Fog;

            uint32_t ConstantBufferIndex{ 0 };
        };

    private:
        template <typename T>
        using LookUpTable = std::unordered_map<std::string, T>;

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void InitConstantBuffers();
        bool InitTextures();
        void InitMaterials();
        bool InitMeshGeometries();
        bool InitRootSignatures();
        bool InitPipelineStates();
        void InitRenderItems();
        void InitLights();

        void InitDepthStencil();

        void UpdateViewport();
        void UpdateScissorRect();

        void RenderFullscreenQuad();

        void Render(
            std::vector<const spieler::RenderItem*> items,
            const spieler::PipelineState& pso,
            const PassConstants& passConstants,
            const spieler::BufferResource* objectConstantBuffer = nullptr);

        bool OnWindowResized(spieler::WindowResizedEvent& event);

    private:
        spieler::GraphicsFormat m_DepthStencilFormat{ spieler::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::Texture m_DepthStencil;

        spieler::Viewport m_Viewport;
        spieler::ScissorRect m_ScissorRect;

        spieler::Camera m_Camera;
        spieler::CameraController m_CameraController{ m_Camera };

        LookUpTable<spieler::Texture> m_Textures;
        LookUpTable<spieler::SamplerConfig> m_Samplers;
        //LookUpTable<spieler::SamplerView> m_SamplerViews;
        LookUpTable<spieler::Material> m_Materials;
        LookUpTable<spieler::MeshGeometry> m_MeshGeometries;
        LookUpTable<spieler::RootSignature> m_RootSignatures;

        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;

        LookUpTable<spieler::GraphicsPipelineState> m_PipelineStates;
        LookUpTable<std::shared_ptr<spieler::BufferResource>> m_ConstantBuffers;

        // RenderItems
        std::unordered_map<std::string, std::unique_ptr<spieler::RenderItem>> m_RenderItems;
        std::vector<const spieler::RenderItem*> m_Lights;
        std::vector<const spieler::RenderItem*> m_OpaqueObjects;
        std::vector<const spieler::RenderItem*> m_Mirrors;
        std::vector<const spieler::RenderItem*> m_ReflectedObjects;
        std::vector<const spieler::RenderItem*> m_AlphaTestedObjects;
        std::vector<const spieler::RenderItem*> m_AlphaTestedBillboards;
        std::vector<const spieler::RenderItem*> m_TransparentObjects;
        std::vector<const spieler::RenderItem*> m_Shadows;
        std::vector<const spieler::RenderItem*> m_BillboardShadows;

        LookUpTable<PassConstants> m_PassConstants;

        DirectionalLightController m_DirectionalLightController;

        std::unique_ptr<BlurPass> m_BlurPass;
        BlurPassExecuteProps m_BlurPassExecuteProps{ 2.5f, 2.5f, 0 };

        std::unique_ptr<SobelFilterPass> m_SobelFilterPass;
        bool m_EnableSobelFilter{ true };
    };
#endif

} // namespace sandbox

#include "test_layer.inl"
