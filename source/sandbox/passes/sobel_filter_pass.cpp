#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <benzin/graphics/api/device.hpp>
#include <benzin/graphics/api/resource_view_builder.hpp>
#include <benzin/graphics/api/graphics_command_list.hpp>
#include <benzin/graphics/api/texture.hpp>
#include <benzin/graphics/api/pipeline_state.hpp>

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
            enum : uint32_t
            {
                RootConstant_InputTextureIndex = 0,
                RootConstant_OutputTextureIndex = 1
            };

            const uint32_t width = input.GetConfig().Width;
            const uint32_t height = input.GetConfig().Height;

            commandList.SetPipelineState(*m_SobelFilterPSO);
            commandList.SetRootConstant(RootConstant_InputTextureIndex, input.GetShaderResourceView().GetHeapIndex(), true);
            commandList.SetRootConstant(RootConstant_OutputTextureIndex, m_EdgeMap->GetUnorderedAccessView().GetHeapIndex(), true);

            commandList.Dispatch(
                benzin::AlignThreadGroupCount(width, g_ThreadPerGroupCount),
                benzin::AlignThreadGroupCount(height, g_ThreadPerGroupCount),
                1
            );
        }
        
        {
            enum : uint32_t
            {
                RootConstant_BaseTextureMapIndex = 0,
                RootConstant_EdgeTextureMapIndex = 1
            };

            commandList.SetResourceBarrier(input, benzin::Resource::State::RenderTarget);

            commandList.SetPipelineState(*m_CompositePSO);
            commandList.SetRootConstant(RootConstant_BaseTextureMapIndex, input.GetShaderResourceView().GetHeapIndex());
            commandList.SetRootConstant(RootConstant_EdgeTextureMapIndex, m_EdgeMap->GetShaderResourceView().GetHeapIndex());

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
            .Format{ benzin::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ benzin::TextureResource::Flags::BindAsUnorderedAccess }
        };

        m_EdgeMap = device.CreateTextureResource(config, "SobelFilterEdgeMap");
        m_EdgeMap->PushShaderResourceView(device.GetResourceViewBuilder().CreateShaderResourceView(*m_EdgeMap));
        m_EdgeMap->PushUnorderedAccessView(device.GetResourceViewBuilder().CreateUnorderedAccessView(*m_EdgeMap));
    }

    void SobelFilterPass::InitPipelineStates(benzin::Device& device)
    {
        {
            const benzin::ComputePipelineState::Config config
            {
                .ComputeShader{ "sobel_filter.hlsl", "CS_Main" }
            };

            m_SobelFilterPSO = std::make_unique<benzin::ComputePipelineState>(device, config, "SobelFilter");
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
