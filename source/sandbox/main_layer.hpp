#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/camera.hpp>
#include <benzin/engine/entity.hpp>
#include <benzin/engine/model.hpp>
#include <benzin/graphics/api/device.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/api/swap_chain.hpp>
#include <benzin/system/window.hpp>

namespace sandbox
{

    class GeometryPass
    {
    public:
        struct GBuffer
        {
            std::shared_ptr<benzin::TextureResource> Albedo;
            std::shared_ptr<benzin::TextureResource> WorldNormal;
            std::shared_ptr<benzin::TextureResource> Emissive;
            std::shared_ptr<benzin::TextureResource> RoughnessMetalness;
            std::shared_ptr<benzin::TextureResource> DepthStencil;
        };

    private:
        struct Resources
        {
            std::unique_ptr<benzin::BufferResource> PassBuffer;
        };

    private:
        static constexpr auto ms_Color0Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_Color1Format = benzin::GraphicsFormat::RGBA16Float;
        static constexpr auto ms_Color2Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_Color3Format = benzin::GraphicsFormat::RGBA8Unorm;
        static constexpr auto ms_DepthStencilFormat = benzin::GraphicsFormat::D24Unorm_S8Uint;

    public:
        GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height);

    public:
        GBuffer& GetGBuffer() { return m_GBuffer; }

    public:
        void OnUpdate(const benzin::Camera& camera);
        void OnExecute(const entt::registry& registry, const benzin::BufferResource& entityBuffer);

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::array<Resources, benzin::config::GetBackBufferCount()> m_Resources;
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
        struct Resources
        {
            std::unique_ptr<benzin::BufferResource> PassBuffer;
            std::unique_ptr<benzin::BufferResource> PointLightBuffer;
        };

    public:
        DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height);

    public:
        benzin::TextureResource& GetOutputTexture() { return *m_OutputTexture; }
        OutputType GetOutputType() const { return m_OutputType; }

    public:
        void OnUpdate(const benzin::Camera& camera, const entt::registry& registry);
        void OnExecute(const GeometryPass::GBuffer& gbuffer);
        void OnImGuiRender();

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::array<Resources, benzin::config::GetBackBufferCount()> m_Resources;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::TextureResource> m_OutputTexture;

        OutputType m_OutputType{ OutputType::Final };

        DirectX::XMFLOAT3 m_SunColor{ 1.0f, 1.0f, 1.0f };
        float m_SunIntensity{ 0.0f, };
        DirectX::XMFLOAT3 m_SunDirection{ -0.5f, -0.5f, -0.5f };
    };

    class EnvironmentPass
    {
    private:
        struct Resources
        {
            std::unique_ptr<benzin::BufferResource> PassBuffer;
        };

    public:
        EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain, uint32_t width, uint32_t height);

    public:
        void OnUpdate(const benzin::Camera& camera);
        void OnExecute(benzin::TextureResource& deferredLightingOutputTexture, benzin::TextureResource& gbufferDepthStecil);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::array<Resources, benzin::config::GetBackBufferCount()> m_Resources;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::TextureResource> m_EnvironmentTexture;
        std::unique_ptr<benzin::TextureResource> m_HDRTexture;

        std::unique_ptr<benzin::TextureResource> m_CubeTexture;
    };

    class MainLayer : public benzin::Layer
    {
    private:
        struct Resources
        {
            std::unique_ptr<benzin::BufferResource> EntityDataBuffer;
        };

    public:
        MainLayer(benzin::Window& window, benzin::Device& device, benzin::SwapChain& swapChain);
        ~MainLayer();

    public:
        bool OnAttach() override;
        bool OnDetach() override;

        void OnEvent(benzin::Event& event) override;
        void OnUpdate(float dt) override;
        void OnRender(float dt) override;
        void OnImGuiRender(float dt) override;

    private:
        void CreateResources();
        void CreateEntities();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::array<Resources, benzin::config::GetBackBufferCount()> m_Resources;

        benzin::PerspectiveProjection m_PerspectiveProjection{ DirectX::XMConvertToRadians(60.0f), m_SwapChain.GetAspectRatio(), 0.1f, 1000.0f };
        benzin::Camera m_Camera{ &m_PerspectiveProjection };
        benzin::FlyCameraController m_FlyCameraController{ &m_Camera };

        entt::registry m_Registry;

        GeometryPass m_GeometryPass;
        DeferredLightingPass m_DeferredLightingPass;
        EnvironmentPass m_EnvironmentPass;
    };

} // namespace sandbox
