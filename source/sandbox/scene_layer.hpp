#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/scene.hpp>

#include <shaders/joint/enum_types.hpp>

namespace benzin
{
    
    class Buffer;
    class Device;
    class GpuTimer;
    class PipelineState;
    class SwapChain;
    class Texture;
    class Window;

    struct GraphicsRefs;

    template <typename ConstnatsT>
    class ConstantBuffer;

} // namespace benzin

namespace sandbox
{

    template <typename T, uint32_t Size>
    class PingPongWrapper
    {
    public:
        auto GetSize() const { return Size; }

        auto& GetCurrent() { return m_Data[m_CurrentIndex]; }
        const auto& GetCurrent() const { return m_Data[m_CurrentIndex]; }

        auto& GetPrevious() { return m_Data[m_PreviousIndex]; }
        const auto& GetPrevious() const { return m_Data[m_PreviousIndex]; }


        void InitializeAll(std::function<void(uint32_t, T&)>&& callback)
        {
            for (const auto& [i, data] : m_Data | std::views::enumerate)
            {
                callback((uint32_t)i, data);
            }
        }

        void GoToNext()
        {
            m_PreviousIndex = m_CurrentIndex;
            m_CurrentIndex = (m_CurrentIndex + 1) % Size;
        }

    private:
        static_assert(Size >= 2);

        std::array<T, Size> m_Data;
        uint32_t m_PreviousIndex = 0;
        uint32_t m_CurrentIndex = 1;
    };

    class GeometryPass
    {
    public:
        struct GBuffer
        {
            std::unique_ptr<benzin::Texture> AlbedoAndRoughness;
            std::unique_ptr<benzin::Texture> EmissiveAndMetallic;
            std::unique_ptr<benzin::Texture> WorldNormal;
            std::unique_ptr<benzin::Texture> VelocityBuffer; // Motion vectors
            std::unique_ptr<benzin::Texture> DepthStencil;

            PingPongWrapper<std::unique_ptr<benzin::Texture>, 2> ViewDepths;
        };

    public:
        GeometryPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetGBuffer() { return m_GBuffer; }

    public:
        void OnUpdate();
        void OnRender(const benzin::Scene& scene) const;

        void OnResize(uint32_t width, uint32_t height);

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        GBuffer m_GBuffer;
    };

    class RtShadowPass
    {
    public:
        RtShadowPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetVisibilityBuffers() const { return m_VisibilityBuffers; }

    public:
        void OnUpdate(std::chrono::microseconds dt, std::chrono::milliseconds elapsedTime);
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer) const;

        void OnResize(uint32_t width, uint32_t height);

    private:
        void CreatePipelineStateObject();
        void CreateShaderTable();

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::RtShadowPassConstants>;

        benzin::Device& m_Device;

        ComPtr<ID3D12StateObject> m_D3D12RaytracingStateObject;

        std::unique_ptr<benzin::Buffer> m_RayGenShaderTable;
        std::unique_ptr<benzin::Buffer> m_MissShaderTable;
        std::unique_ptr<benzin::Buffer> m_HitGroupShaderTable;

        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
        PingPongWrapper<std::unique_ptr<benzin::Texture>, 2> m_VisibilityBuffers;
    };

    class RtShadowDenoisingPass
    {
    public:
        RtShadowDenoisingPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetTemporalAccumulationBuffers() { return m_TemporalAccumulationBuffers; }
        auto& GetDenoisedVisibilityBuffer() { return *m_DenoisedVisibilityBuffer; }

    public:
        void OnUpdate();
        void OnRender(const GeometryPass::GBuffer& gbuffer, const benzin::Texture& previousVisibilityBuffer, const benzin::Texture& currentVisiblityBuffer) const;

        void OnResize(uint32_t width, uint32_t height);

    private:
        void RunTemporalAccumulationSubPass(const GeometryPass::GBuffer& gbuffer, const benzin::Texture& previousVisibilityBuffer, const benzin::Texture& currentVisiblityBuffer) const;
        void RunMipGenerationSubPass(const GeometryPass::GBuffer& gbuffer) const;

    private:
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        PingPongWrapper<std::unique_ptr<benzin::Texture>, 2> m_TemporalAccumulationBuffers;
        std::unique_ptr<benzin::Texture> m_DenoisedVisibilityBuffer;
    };

    class DeferredLightingPass
    {
    public:
        DeferredLightingPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        auto& GetOutputTexture() { return *m_OutputTexture; }

    public:
        void OnUpdate(const benzin::Scene& scene);
        void OnRender(const benzin::Scene& scene, const GeometryPass::GBuffer& gbuffer, benzin::Texture& shadowVisiblityBuffer) const;

        void OnResize(uint32_t width, uint32_t height);

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::DeferredLightingPassConstants>;

        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
        std::unique_ptr<benzin::Texture> m_OutputTexture;
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

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<benzin::Texture> m_CubeTexture;
    };

    class FullScreenDebugPass
    {
    public:
        FullScreenDebugPass(benzin::Device& device, benzin::SwapChain& swapChain);

    public:
        void OnUpdate();
        void OnRender(benzin::Texture& finalOutput, const GeometryPass::GBuffer& gbuffer, benzin::Texture& shadowVisiblityBuffer, benzin::Texture& temporalAccumulationBuffer) const;

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::FullScreenDebugConstants>;

        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::PipelineState> m_Pso;
        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
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
        using FrameConstantBuffer = benzin::ConstantBuffer<joint::FrameConstants>;

        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        std::unique_ptr<benzin::GpuTimer> m_GpuTimer;
        std::unique_ptr<FrameConstantBuffer> m_FrameConstantBuffer;

        GeometryPass m_GeometryPass;
        RtShadowPass m_RtShadowPass;
        RtShadowDenoisingPass m_RtShadowDenoisingPass;
        DeferredLightingPass m_DeferredLightingPass;
        EnvironmentPass m_EnvironmentPass;
        FullScreenDebugPass m_FullScreenDebugPass;

        bool m_IsAnimationEnabled = true;

        benzin::Scene m_Scene{ m_Device };
        benzin::FlyCameraController m_FlyCameraController{ m_Scene.GetCamera() };
    };

} // namespace sandbox
