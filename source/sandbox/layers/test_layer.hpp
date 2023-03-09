#pragma once

#if 0

#include <benzin/core/layer.hpp>

#include <benzin/system/window.hpp>

#include <benzin/graphics/shader.hpp>
#include <benzin/graphics/root_signature.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/sampler.hpp>
#include <benzin/graphics/graphics_command_list.hpp>

#include <benzin/engine/mesh.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/camera.hpp>

#include "passes/blur_pass.hpp"
#include "passes/sobel_filter_pass.hpp"

namespace sandbox
{

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

    class TestLayer final : public benzin::Layer
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
            std::array<benzin::LightConstants, g_MaxLightCount> Lights;

            Fog Fog;

            uint32_t ConstantBufferIndex{ 0 };
        };

    private:
        template <typename T>
        using LookUpTable = std::unordered_map<std::string, T>;

    public:
        bool OnAttach() override;

        void OnEvent(benzin::Event& event) override;
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
            std::vector<const benzin::RenderItem*> items,
            const benzin::PipelineState& pso,
            const PassConstants& passConstants,
            const benzin::BufferResource* objectConstantBuffer = nullptr);

        bool OnWindowResized(benzin::WindowResizedEvent& event);

    private:
        benzin::GraphicsFormat m_DepthStencilFormat{ benzin::GraphicsFormat::D24UnsignedNormS8UnsignedInt };
        benzin::Texture m_DepthStencil;

        benzin::Viewport m_Viewport;
        benzin::ScissorRect m_ScissorRect;

        benzin::Camera m_Camera;
        benzin::CameraController m_CameraController{ m_Camera };

        LookUpTable<benzin::Texture> m_Textures;
        LookUpTable<benzin::SamplerConfig> m_Samplers;
        //LookUpTable<benzin::SamplerView> m_SamplerViews;
        LookUpTable<benzin::Material> m_Materials;
        LookUpTable<benzin::MeshGeometry> m_MeshGeometries;
        LookUpTable<benzin::RootSignature> m_RootSignatures;

        std::unordered_map<std::string, std::shared_ptr<benzin::Shader>> m_ShaderLibrary;

        LookUpTable<benzin::GraphicsPipelineState> m_PipelineStates;
        LookUpTable<std::shared_ptr<benzin::BufferResource>> m_ConstantBuffers;

        // RenderItems
        std::unordered_map<std::string, std::unique_ptr<benzin::RenderItem>> m_RenderItems;
        std::vector<const benzin::RenderItem*> m_Lights;
        std::vector<const benzin::RenderItem*> m_OpaqueObjects;
        std::vector<const benzin::RenderItem*> m_Mirrors;
        std::vector<const benzin::RenderItem*> m_ReflectedObjects;
        std::vector<const benzin::RenderItem*> m_AlphaTestedObjects;
        std::vector<const benzin::RenderItem*> m_AlphaTestedBillboards;
        std::vector<const benzin::RenderItem*> m_TransparentObjects;
        std::vector<const benzin::RenderItem*> m_Shadows;
        std::vector<const benzin::RenderItem*> m_BillboardShadows;

        LookUpTable<PassConstants> m_PassConstants;

        DirectionalLightController m_DirectionalLightController;

        std::unique_ptr<BlurPass> m_BlurPass;
        BlurPassExecuteProps m_BlurPassExecuteProps{ 2.5f, 2.5f, 0 };

        std::unique_ptr<SobelFilterPass> m_SobelFilterPass;
        bool m_EnableSobelFilter{ true };
    };

} // namespace sandbox

#include "test_layer.inl"

#endif
