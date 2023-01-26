#pragma once

#include <spieler/system/window_event.hpp>

#include <spieler/graphics/texture.hpp>
#include <spieler/graphics/root_signature.hpp>
#include <spieler/graphics/pipeline_state.hpp>
#include <spieler/graphics/shader.hpp>
#include <spieler/graphics/graphics_command_list.hpp>
#include <spieler/graphics/device.hpp>

namespace sandbox
{

    struct BlurPassExecuteProps
    {
        float HorizontalBlurSigma{ 0.0f };
        float VerticalBlurSigma{ 0.0f };
        uint32_t BlurCount{ 0 };
    };

    class BlurPass
    {
    public:
        static int32_t GetBlurRadius(float sigma) { return static_cast<int32_t>(std::ceilf(sigma * 2.0f)); }

    public:
        BlurPass(spieler::Device& device, uint32_t width, uint32_t height);

    public:
        std::shared_ptr<spieler::TextureResource>& GetOutput() { return m_BlurMaps[0]; }

    public:
        void OnExecute(spieler::GraphicsCommandList& graphicsCommandList, spieler::TextureResource& input, const BlurPassExecuteProps& props);
        void OnResize(spieler::Device& device, uint32_t width, uint32_t height);

    private:
        void InitTextures(spieler::Device& device, uint32_t width, uint32_t height);
        void InitRootSignature(spieler::Device& device);
        void InitPSOs(spieler::Device& device);

    private:
        spieler::RootSignature m_RootSignature;
        spieler::PipelineState m_HorizontalPSO;
        spieler::PipelineState m_VerticalPSO;

        std::array<std::shared_ptr<spieler::TextureResource>, 2> m_BlurMaps;
        std::unordered_map<std::string, std::shared_ptr<spieler::Shader>> m_ShaderLibrary;
    };

} // namespace sandbox
