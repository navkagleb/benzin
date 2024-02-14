#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/scene.hpp>

#include <shaders/joint/enum_types.hpp>

namespace benzin
{
    
    class Buffer;
    class Device;
    class GPUTimer;
    class PipelineState;
    class SwapChain;
    class Texture;
    class Window;

    struct GraphicsRefs;

} // namespace benzin

namespace sandbox
{

    class GeometryPass
    {
    public:
        struct GBuffer
        {
            std::unique_ptr<benzin::Texture> Albedo;
            std::unique_ptr<benzin::Texture> WorldNormal;
            std::unique_ptr<benzin::Texture> Emissive;
            std::unique_ptr<benzin::Texture> RoughnessMetalness;
            std::unique_ptr<benzin::Texture> DepthStencil;
        };

    public:
        GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetGBuffer() { return m_GBuffer; }

    public:
        void OnRender(const benzin::Scene& scene);

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        GBuffer m_GBuffer;
    };

    class RTShadowPass
    {
    public:
        RTShadowPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        const auto& GetOutputTexture() const { return *m_OutputTexture; }

        void SetRaysPerPixel(uint32_t raysPerPixel) { m_RaysPerPixel = raysPerPixel; }

    public:
        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime);
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer);

    private:
        void CreatePipelineStateObject();
        void CreateShaderTable();

    private:
        benzin::Device& m_Device;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        std::unique_ptr<benzin::Buffer> m_PassConstantBuffer;
        std::unique_ptr<benzin::Texture> m_OutputTexture;

        uint32_t m_RaysPerPixel = 1;
    };

    class DeferredLightingPass
    {
    public:
        DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetOutputTexture() { return *m_OutputTexture; }
        auto GetOutputType() const { return m_OutputType; }

    public:
        void OnUpdate(const benzin::Scene& scene);
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer, const benzin::Texture& shadowTexture);
        void OnImGuiRender();

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::Buffer> m_PassConstantBuffer;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::Texture> m_OutputTexture;

        joint::DeferredLightingOutputType m_OutputType = joint::DeferredLightingOutputType_Final;

        DirectX::XMFLOAT3 m_SunColor{ 1.0f, 1.0f, 1.0f };
        float m_SunIntensity{ 0.0f, };
        DirectX::XMFLOAT3 m_SunDirection{ -0.5f, -0.5f, -0.5f };
    };

    class EnvironmentPass
    {
    public:
        EnvironmentPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        void OnRender(const benzin::Scene& scene, benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_PipelineState;
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class SceneLayer : public benzin::Layer
    {
    public:
        explicit SceneLayer(const benzin::GraphicsRefs& graphicsRefs);
        ~SceneLayer();

    public:
        void OnEndFrame() override;

        void OnEvent(benzin::Event& event) override;
        void OnUpdate() override;
        void OnRender() override;
        void OnImGuiRender() override;

    private:
        void CreateEntities();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::GPUTimer> m_GPUTimer;

        benzin::Scene m_Scene{ m_Device };
        benzin::FlyCameraController m_FlyCameraController{ m_Scene.GetCamera() };

        entt::entity m_DamagedHelmetEntity = benzin::g_InvalidEntity;
        entt::entity m_BoomBoxEntity = benzin::g_InvalidEntity;

        GeometryPass m_GeometryPass;
        RTShadowPass m_RTShadowPass;
        DeferredLightingPass m_DeferredLightingPass;
        EnvironmentPass m_EnvironmentPass;

        bool m_IsAnimationEnabled = true;
    };

} // namespace sandbox
