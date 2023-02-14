#pragma once

#include <benzin/graphics/api/common.hpp>

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
        BlurPass(benzin::Device& device, uint32_t width, uint32_t height);

    public:
        std::shared_ptr<benzin::TextureResource>& GetOutput() { return m_BlurMaps[0]; }

    public:
        void OnExecute(benzin::GraphicsCommandList& graphicsCommandList, benzin::TextureResource& input, const BlurPassExecuteProps& props);
        void OnResize(benzin::Device& device, uint32_t width, uint32_t height);

    private:
        void InitTextures(benzin::Device& device, uint32_t width, uint32_t height);
        void InitRootSignature(benzin::Device& device);
        void InitPSOs(benzin::Device& device);

    private:
        std::unique_ptr<benzin::RootSignature> m_RootSignature;
        std::unique_ptr<benzin::PipelineState> m_HorizontalPSO;
        std::unique_ptr<benzin::PipelineState> m_VerticalPSO;

        std::array<std::shared_ptr<benzin::TextureResource>, 2> m_BlurMaps;
        std::unordered_map<std::string, std::unique_ptr<benzin::Shader>> m_ShaderLibrary;
    };

} // namespace sandbox
