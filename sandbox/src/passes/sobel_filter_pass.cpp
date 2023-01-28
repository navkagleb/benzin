#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/resource_view_builder.hpp>

namespace sandbox
{

    SobelFilterPass::SobelFilterPass(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitOutputTexture(device, width, height);
        InitRootSignature(device);
        InitPipelineState(device);
    }

    void SobelFilterPass::Execute(benzin::GraphicsCommandList& graphicsCommandList, benzin::TextureResource& input)
    {
        const uint32_t width = input.GetConfig().Width;
        const uint32_t height = input.GetConfig().Height;

        graphicsCommandList.SetPipelineState(m_PipelineState);
        graphicsCommandList.SetComputeRootSignature(m_RootSignature);

        graphicsCommandList.SetComputeDescriptorTable(0, input.GetShaderResourceView());
        graphicsCommandList.SetComputeDescriptorTable(1, m_OutputTexture->GetUnorderedAccessView());

        const uint32_t threadGroupXCount = width / 16 + 1;
        const uint32_t threadGroupYCount = height / 16 + 1;
        graphicsCommandList.Dispatch(threadGroupXCount, threadGroupYCount, 1);
    }

    void SobelFilterPass::OnResize(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitOutputTexture(device, width, height);
    }

    void SobelFilterPass::InitOutputTexture(benzin::Device& device, uint32_t width, uint32_t height)
    {
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .Format{ benzin::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ benzin::TextureResource::Flags::BindAsUnorderedAccess }
        };

        m_OutputTexture = device.CreateTextureResource(config, "SobelFilterOutput");
        m_OutputTexture->PushShaderResourceView(device.GetResourceViewBuilder().CreateUnorderedAccessView(*m_OutputTexture));
    }

    void SobelFilterPass::InitRootSignature(benzin::Device& device)
    {
        benzin::RootSignature::Config config;

        config.RootParameters.emplace_back(benzin::RootParameter::SingleDescriptorTableConfig
        {
            .Range
            {
                .Type{ benzin::Descriptor::Type::ShaderResourceView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        });

        config.RootParameters.emplace_back(benzin::RootParameter::SingleDescriptorTableConfig
        {
            .Range
            {
                .Type{ benzin::Descriptor::Type::UnorderedAccessView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        });

        m_RootSignature = benzin::RootSignature{ device, config };
    }

    void SobelFilterPass::InitPipelineState(benzin::Device& device)
    {
        const benzin::Shader::Config shaderConfig
        {
            .Type{ benzin::Shader::Type::Compute },
            .Filepath{ L"assets/shaders/sobel_filter.hlsl" },
            .EntryPoint{ "CS_Main" }
        };

        m_Shader = benzin::Shader::Create(shaderConfig);

        const benzin::ComputePipelineState::Config pipelineStateConfig
        {
            .RootSignature{ &m_RootSignature },
            .ComputeShader{ m_Shader.get() }
        };

        m_PipelineState = benzin::ComputePipelineState(device, pipelineStateConfig);
    }

} // namespace sandbox
