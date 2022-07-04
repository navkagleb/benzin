#pragma once

#include <spieler/core/layer.hpp>

#include <spieler/system/window.hpp>

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
#include "passes/blur_pass.hpp"
#include "passes/sobel_filter_pass.hpp"

namespace sandbox
{
    template <typename T>
    using LookUpTable = std::unordered_map<std::string, T>;

    constexpr uint32_t MAX_LIGHT_COUNT{ 16 };

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
        std::array<spieler::renderer::LightConstants, MAX_LIGHT_COUNT> Lights;

        Fog Fog;
    };

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

    class TestLayer : public spieler::Layer
    {
    public:
        TestLayer(spieler::Window& window);

    public:
        bool OnAttach() override;

        void OnEvent(spieler::Event& event) override;
        void OnUpdate(float dt) override;
        bool OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void InitConstantBuffers();
        bool InitTextures(spieler::renderer::UploadBuffer& uploadBuffer);
        void InitMaterials();
        bool InitMeshGeometries(spieler::renderer::UploadBuffer& uploadBuffer);
        bool InitRootSignatures();
        bool InitPipelineStates();
        void InitRenderItems();
        void InitLights();

        void InitDepthStencil();

        template <typename Permutations>
        const spieler::renderer::Shader& GetShader(spieler::renderer::ShaderType type, const spieler::renderer::ShaderPermutation<Permutations>& permutation);

        void UpdateViewport();
        void UpdateScissorRect();

        void RenderFullscreenQuad();

        void Render(
            std::vector<const spieler::renderer::RenderItem*> items,
            const spieler::renderer::PipelineState& pso,
            const PassConstants& passConstants,
            const spieler::renderer::ConstantBuffer* objectConstantBuffer = nullptr);

        bool OnWindowResized(spieler::WindowResizedEvent& event);

    private:
        spieler::Window& m_Window;

        spieler::renderer::GraphicsFormat m_DepthStencilFormat{ spieler::renderer::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        spieler::renderer::Texture2D m_DepthStencil;

        spieler::renderer::Viewport m_Viewport;
        spieler::renderer::ScissorRect m_ScissorRect;

        ProjectionCameraController m_CameraController;

        LookUpTable<spieler::renderer::Texture2D> m_Textures;
        LookUpTable<spieler::renderer::SamplerConfig> m_Samplers;
        LookUpTable<spieler::renderer::SamplerView> m_SamplerViews;
        LookUpTable<spieler::renderer::Material> m_Materials;
        LookUpTable<spieler::renderer::MeshGeometry> m_MeshGeometries;
        LookUpTable<spieler::renderer::RootSignature> m_RootSignatures;

        spieler::renderer::ShaderLibrary m_ShaderLibrary;

        LookUpTable<spieler::renderer::GraphicsPipelineState> m_PipelineStates;
        LookUpTable<spieler::renderer::ConstantBuffer> m_ConstantBuffers;

        // RenderItems
        std::unordered_map<std::string, std::unique_ptr<spieler::renderer::RenderItem>> m_RenderItems;
        std::vector<const spieler::renderer::RenderItem*> m_Lights;
        std::vector<const spieler::renderer::RenderItem*> m_OpaqueObjects;
        std::vector<const spieler::renderer::RenderItem*> m_Mirrors;
        std::vector<const spieler::renderer::RenderItem*> m_ReflectedObjects;
        std::vector<const spieler::renderer::RenderItem*> m_AlphaTestedObjects;
        std::vector<const spieler::renderer::RenderItem*> m_AlphaTestedBillboards;
        std::vector<const spieler::renderer::RenderItem*> m_TransparentObjects;
        std::vector<const spieler::renderer::RenderItem*> m_Shadows;
        std::vector<const spieler::renderer::RenderItem*> m_BillboardShadows;

        LookUpTable<PassConstants> m_PassConstants;

        DirectionalLightController m_DirectionalLightController;

        BlurPass m_BlurPass;
        BlurPassExecuteProps m_BlurPassExecuteProps{ 2.5f, 2.5f, 0 };

        SobelFilterPass m_SobelFilterPass;
        bool m_EnableSobelFilter{ true };
    };

} // namespace sandbox

#include "test_layer.inl"