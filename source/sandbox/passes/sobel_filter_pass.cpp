#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <benzin/graphics/api/device.hpp>
#include <benzin/graphics/api/command_list.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>
#include <benzin/graphics/api/texture.hpp>

#if 0

namespace sandbox
{

    constexpr uint32_t g_ThreadPerGroupCount = 16;

    SobelFilterPass::SobelFilterPass(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
        InitPipelineStates(device);
    }

    void SobelFilterPass::OnExecute(benzin::GraphicsCommandList& commandList, benzin::TextureResource& input)
    {
        BENZIN_ASSERT(input.HasShaderResourceView());
        BENZIN_ASSERT(input.HasRenderTargetView());

        {
            enum class RootConstant : uint32_t
            {
                InputTextureIndex,
                OutputTextureIndex,
            };

            const uint32_t width = input.GetConfig().Width;
            const uint32_t height = input.GetConfig().Height;

            commandList.SetPipelineState(*m_SobelFilterPSO);
            commandList.SetRootShaderResourceC(RootConstant::InputTextureIndex, input.GetShaderResourceView());
            commandList.SetRootUnorderedAccessC(RootConstant::OutputTextureIndex, m_EdgeMap->GetUnorderedAccessView());

            commandList.Dispatch(
                benzin::AlignThreadGroupCount(width, g_ThreadPerGroupCount),
                benzin::AlignThreadGroupCount(height, g_ThreadPerGroupCount),
                1
            );
        }
        
        {
            enum class RootConstant : uint32_t
            {
                BaseTextureMapIndex,
                EdgeTextureMapIndex,
            };

            commandList.SetResourceBarrier(input, benzin::Resource::State::RenderTarget);

            commandList.SetPipelineState(*m_CompositePSO);
            commandList.SetRootShaderResourceG(RootConstant::BaseTextureMapIndex, input.GetShaderResourceView());
            commandList.SetRootShaderResourceG(RootConstant::EdgeTextureMapIndex, m_EdgeMap->GetShaderResourceView());

            commandList.DrawVertexed(6, 0);

            commandList.SetResourceBarrier(input, benzin::Resource::State::Present);
        }
    }

    void SobelFilterPass::OnResize(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
    }

    void SobelFilterPass::InitTextures(benzin::Device& device, uint32_t width, uint32_t height)
    {
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .Format{ benzin::GraphicsFormat::RGBA8Unorm },
            .Flags{ benzin::TextureResource::Flags::BindAsUnorderedAccess }
        };

        m_EdgeMap = std::make_shared<benzin::TextureResource>(device, config);
        m_EdgeMap->SetDebugName("SobelFilterEdgeMap");
        m_EdgeMap->PushShaderResourceView();
        m_EdgeMap->PushUnorderedAccessView();
    }

    void SobelFilterPass::InitPipelineStates(benzin::Device& device)
    {
        {
            const benzin::ComputePipelineState::Config config
            {
                .ComputeShader{ "sobel_filter.hlsl", "CS_Main" }
            };

            m_SobelFilterPSO = std::make_unique<benzin::ComputePipelineState>(device, config);
            m_SobelFilterPSO->SetDebugName("SobelFilter");
        }
        
        {
            const benzin::GraphicsPipelineState::Config config
            {
                .VertexShader{ "sobel_filter_composite.hlsl", "VS_Main" },
                .PixelShader{ "sobel_filter_composite.hlsl", "PS_Main" },
                .DepthState{ benzin::DepthState::GetDisabled() },
                .PrimitiveTopologyType{ benzin::PrimitiveTopologyType::Triangle },
                .RenderTargetViewFormats{ benzin::config::GetBackBufferFormat() }
            };

            m_CompositePSO = std::make_unique<benzin::GraphicsPipelineState>(device, config, "CompositeSobelFilter");
        }
    }

} // namespace sandbox

#endif
