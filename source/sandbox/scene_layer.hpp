#pragma once

#include <benzin/core/layer.hpp>
#include <benzin/engine/scene.hpp>

namespace benzin
{
    
    class Buffer;
    class Device;
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

    struct RenderResources
    {
        // GBuffer
        std::unique_ptr<benzin::Texture> AlbedoAndRoughness;
        std::unique_ptr<benzin::Texture> EmissiveAndMetallic;
        std::unique_ptr<benzin::Texture> WorldNormal;
        std::unique_ptr<benzin::Texture> VelocityBuffer;
        std::unique_ptr<benzin::Texture> DepthStencil;
        std::unique_ptr<benzin::Texture> ViewDepths[2];

        std::unique_ptr<benzin::Texture> NoisyShadowVisibilityBuffers[2];

        std::unique_ptr<benzin::Texture> TemporalAccumulationBuffers[2];
        std::unique_ptr<benzin::Texture> ReprojectedHistoryTexture;
        std::unique_ptr<benzin::Texture> DenoisedShadowVisibilityBuffers[2];

        std::unique_ptr<benzin::Texture> FinalOutputTexture;

        uint32_t PreviousFlipResourceIndex = 1;
        uint32_t ActiveFlipResourceIndex = 0;

        void FlipResources()
        {
            PreviousFlipResourceIndex = ActiveFlipResourceIndex;
            ActiveFlipResourceIndex = (ActiveFlipResourceIndex + 1) % 2;
        }

        auto& GetPreviousResource(std::unique_ptr<benzin::Texture> flipTextures[2]) { return flipTextures[PreviousFlipResourceIndex]; }
        auto& GetCurrentResource(std::unique_ptr<benzin::Texture> flipTextures[2]) { return flipTextures[ActiveFlipResourceIndex]; }
    };

    class RenderPass;

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

        void OnResize(uint32_t width, uint32_t height) override;

    private:
        void LoadAndCreateEntities();

    private:
        benzin::Window& m_Window;
        benzin::Device& m_Device;
        benzin::SwapChain& m_SwapChain;

        bool m_IsAnimationEnabled = true;

        benzin::Scene m_Scene{ m_Device };
        benzin::FlyCameraController m_FlyCameraController{ m_Scene.GetCamera() };

        using FrameConstantBuffer = benzin::ConstantBuffer<joint::FrameConstants>;
        std::unique_ptr<FrameConstantBuffer> m_FrameConstantBuffer;

        RenderResources m_RenderResources;
        std::list<std::unique_ptr<RenderPass>> m_RenderPasses;
    };

} // namespace sandbox
