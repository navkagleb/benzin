#pragma once

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/command_list.hpp>

namespace sandbox
{

    class BlurPass
    {
    public:
        struct ExecuteArgs
        {
            float HorizontalBlurSigma = 0.0f;
            float VerticalBlurSigma = 0.0f;
            uint32_t BlurCount = 0;
        };

    private:
        struct DirectionPass
        {
            std::shared_ptr<benzin::Buffer> SettingsBuffer;
            std::unique_ptr<benzin::PipelineState> PipelineState;
        };

    public:
        static int32_t GetBlurRadius(float sigma) { return static_cast<int32_t>(std::ceilf(sigma * 2.0f)); }

    public:
        BlurPass(benzin::Device& device, uint32_t width, uint32_t height);

    public:
        std::shared_ptr<benzin::Texture>& GetOutput() { return m_BlurMaps[0]; }

    public:
        void OnExecute(benzin::GraphicsCommandList& commandList, benzin::Texture& input, const ExecuteArgs& args);
        void OnResize(benzin::Device& device, uint32_t width, uint32_t height);

    private:
        void InitPasses(benzin::Device& device);
        void InitTextures(benzin::Device& device, uint32_t width, uint32_t height);

        void ExecuteDirectionPass(
            benzin::GraphicsCommandList& commandList, 
            benzin::Texture& input, 
            benzin::Texture& output,
            const DirectionPass& pass,
            uint32_t threadGroupCountX,
            uint32_t threadGroupCountY
        );

    private:
        DirectionPass m_HorizontalPass;
        DirectionPass m_VerticalPass;

        std::array<std::shared_ptr<benzin::Texture>, 2> m_BlurMaps;
    };

} // namespace sandbox
