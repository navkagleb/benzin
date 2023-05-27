#pragma once

#include <benzin/graphics/api/device.hpp>
#include <benzin/graphics/api/command_list.hpp>

namespace sandbox
{

    class SobelFilterPass
    {
    public:
        SobelFilterPass(benzin::Device& device, uint32_t width, uint32_t height);

    public:
        const std::shared_ptr<benzin::TextureResource>& GetEdgeMap() const { return m_EdgeMap; }

    public:
        void OnExecute(benzin::GraphicsCommandList& commandList, benzin::TextureResource& input);

        void OnResize(benzin::Device& device, uint32_t width, uint32_t height);

    private:
        void InitTextures(benzin::Device& device, uint32_t width, uint32_t height);
        void InitPipelineStates(benzin::Device& device);

    private:
        std::shared_ptr<benzin::TextureResource> m_EdgeMap;
        std::unique_ptr<benzin::PipelineState> m_SobelFilterPSO;
        std::unique_ptr<benzin::PipelineState> m_CompositePSO;
    };

} // namespace sandbox
