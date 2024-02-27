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
            std::unique_ptr<benzin::Texture> MotionVectors;
            std::unique_ptr<benzin::Texture> DepthStencil;
        };

    public:
        GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetGBuffer() { return m_GBuffer; }

    public:
        void OnRender(const benzin::Scene& scene) const;

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
        auto& GetNoisyVisibilityBuffer() const { return *m_NoisyVisibilityBuffer; }
        auto GetCurrentTextureSlot() const { return m_CurrentTextureSlot; }

        void SetRaysPerPixel(uint32_t raysPerPixel) { m_RaysPerPixel = raysPerPixel; }

    public:
        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime);
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer) const;

        void OnResize(uint32_t width, uint32_t height);

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
        std::unique_ptr<benzin::Texture> m_NoisyVisibilityBuffer;

        uint32_t m_CurrentTextureSlot = 1; // First call of OnUpdate update the value to 0
        uint32_t m_RaysPerPixel = 1;
    };

    class RTShadowDenoisingPass
    {
    public:
        RTShadowDenoisingPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        void SetIsForceCurrentVisibility(bool isForceCurrentVisibility) { m_IsForceCurrentVisibility = isForceCurrentVisibility; }

    public:
        void OnUpdate(uint32_t currentTextureSlot);
        void OnRender(benzin::Texture& noisyVisiblityBuffer, const benzin::Texture& motionVectorsTexture, const benzin::Texture& albedo) const;

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::Buffer> m_PassConstantBuffer;
        std::unique_ptr<benzin::PipelineState> m_PipelineState;

        bool m_IsForceCurrentVisibility = false;
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
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer, benzin::Texture& shadowTexture) const;
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
        void OnRender(const benzin::Scene& scene, benzin::Texture& deferredLightingOutputTexture, benzin::Texture& gbufferDepthStecil) const;

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
        void OnEvent(benzin::Event& event) override;
        void OnUpdate() override;
        void OnRender() override;
        void OnImGuiRender() override;

        void OnResize(uint32_t width, uint32_t height);

    private:
        void CreateEntities();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::GPUTimer> m_GPUTimer;

        GeometryPass m_GeometryPass;
        RTShadowPass m_RTShadowPass;
        RTShadowDenoisingPass m_RTShadowDenoisingPass;
        DeferredLightingPass m_DeferredLightingPass;
        EnvironmentPass m_EnvironmentPass;

        bool m_IsAnimationEnabled = true;

        benzin::Scene m_Scene{ m_Device };
        benzin::FlyCameraController m_FlyCameraController{ m_Scene.GetCamera() };
    };

} // namespace sandbox
