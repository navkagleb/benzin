#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/entity.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/swap_chain.hpp>
#include <benzin/system/window.hpp>

namespace benzin
{

    class GPUTimer;

} // namespace benzin

namespace sandbox
{

    class GeometryPass
    {
    public:
        struct GBuffer
        {
            std::shared_ptr<benzin::Texture> Albedo;
            std::shared_ptr<benzin::Texture> WorldNormal;
            std::shared_ptr<benzin::Texture> Emissive;
            std::shared_ptr<benzin::Texture> RoughnessMetalness;
            std::shared_ptr<benzin::Texture> DepthStencil;
        };

    private:
        struct FrameResources
        {
            std::unique_ptr<benzin::Buffer> PassBuffer;
        };

    private:
        static constexpr auto ms_Color0Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_Color1Format = benzin::GraphicsFormat::RGBA16Float;
        static constexpr auto ms_Color2Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_Color3Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

    public:
        GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer);

    public:
        GBuffer& GetGBuffer() { return m_GBuffer; }

    public:
        void OnUpdate(const benzin::Camera& camera);
        void OnExecute(const entt::registry& registry, const benzin::Buffer& entityBuffer);

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;
        benzin::GPUTimer& m_GPUTimer;

        FrameResources m_FrameResources;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        GBuffer m_GBuffer;
    };

    class DeferredLightingPass
    {
    public:
        enum class OutputType : uint32_t
        {
            Final,
            ReconsructedWorldPosition,
            GBuffer_Depth,
            GBuffer_Albedo,
            GBuffer_WorldNormal,
            GBuffer_Emissive,
            GBuffer_Roughness,
            GBuffer_Metalness,
        };

    private:
        struct FrameResources
        {
            std::unique_ptr<benzin::Buffer> PassBuffer;
            std::unique_ptr<benzin::Buffer> PointLightBuffer;
        };

    private:
        static const uint32_t ms_MaxPointLightCount = 20 * 20;

    public:
        DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer);

    public:
        benzin::Texture& GetOutputTexture() { return *m_OutputTexture; }
        OutputType GetOutputType() const { return m_OutputType; }

    public:
        void OnUpdate(const benzin::Camera& camera, const entt::registry& registry);
        void OnExecute(const GeometryPass::GBuffer& gbuffer);
        void OnImGuiRender();

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;
        benzin::GPUTimer& m_GPUTimer;

        FrameResources m_FrameResources;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::Texture> m_OutputTexture;

        OutputType m_OutputType{ OutputType::Final };

        DirectX::XMFLOAT3 m_SunColor{ 1.0f, 1.0f, 1.0f };
        float m_SunIntensity{ 0.0f, };
        DirectX::XMFLOAT3 m_SunDirection{ -0.5f, -0.5f, -0.5f };
    };

    class EnvironmentPass
    {
    private:
        struct FrameResources
        {
            std::unique_ptr<benzin::Buffer> PassBuffer;
        };

    public:
        EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain, benzin::GPUTimer& gpuTimer);

    public:
        void OnUpdate(const benzin::Camera& camera);
        void OnExecute(benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;
        benzin::GPUTimer& m_GPUTimer;

        FrameResources m_FrameResources;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::Texture> m_EnvironmentTexture;
        std::unique_ptr<benzin::Texture> m_HDRTexture;

        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class MainLayer : public benzin::Layer
    {
    private:
        struct FrameResources
        {
            std::unique_ptr<benzin::Buffer> EntityDataBuffer;
        };

    private:
        static const uint32_t ms_MaxEntityCount = 20 * 20 + 5; // TODO: Remove hardcoded value

    public:
        explicit MainLayer(const benzin::GraphicsRefs& graphicsRefs);
        ~MainLayer();

    public:
        void OnEndFrame() override;

        void OnEvent(benzin::Event& event) override;
        void OnUpdate() override;
        void OnRender() override;
        void OnImGuiRender() override;

    private:
        void CreateResources();
        void CreateEntities();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::GPUTimer> m_GPUTimer;

        FrameResources m_FrameResources;

        benzin::PerspectiveProjection m_PerspectiveProjection{ DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f };
        benzin::Camera m_Camera{ &m_PerspectiveProjection };
        benzin::FlyCameraController m_FlyCameraController{ m_Camera };

        entt::registry m_Registry;

        GeometryPass m_GeometryPass;
        DeferredLightingPass m_DeferredLightingPass;
        EnvironmentPass m_EnvironmentPass;

        entt::entity m_DamagedHelmetEntity;
        entt::entity m_BoomBoxEntity;
    };

} // namespace sandbox
