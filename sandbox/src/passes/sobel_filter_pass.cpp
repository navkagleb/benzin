#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <spieler/graphics/device.hpp>
#include <spieler/graphics/resource_view_builder.hpp>

namespace sandbox
{

    SobelFilterPass::SobelFilterPass(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitOutputTexture(device, width, height);
        InitRootSignature(device);
        InitPipelineState(device);
    }

    void SobelFilterPass::Execute(spieler::GraphicsCommandList& graphicsCommandList, spieler::TextureResource& input)
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

    void SobelFilterPass::OnResize(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitOutputTexture(device, width, height);
    }

    void SobelFilterPass::InitOutputTexture(spieler::Device& device, uint32_t width, uint32_t height)
    {
        const spieler::TextureResource::Config config
        {
            .Type{ spieler::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ spieler::TextureResource::Flags::BindAsUnorderedAccess }
        };

        m_OutputTexture = device.CreateTextureResource(config);
        m_OutputTexture->PushShaderResourceView(device.GetResourceViewBuilder().CreateUnorderedAccessView(*m_OutputTexture));
        m_OutputTexture->SetName("SobelFilterOutputTexture");
    }

    void SobelFilterPass::InitRootSignature(spieler::Device& device)
    {
        const spieler::RootParameter::SingleDescriptorTable srvTable
        {
            .Range
            {
                .Type{ spieler::RootParameter::DescriptorRangeType::ShaderResourceView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        const spieler::RootParameter::SingleDescriptorTable uavTable
        {
            .Range
            {
                .Type{ spieler::RootParameter::DescriptorRangeType::UnorderedAccessView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        spieler::RootSignature::Config rootSignatureConfig{ 2 };
        rootSignatureConfig.RootParameters[0] = srvTable;
        rootSignatureConfig.RootParameters[1] = uavTable;

        m_RootSignature = spieler::RootSignature{ device, rootSignatureConfig };
    }

    void SobelFilterPass::InitPipelineState(spieler::Device& device)
    {
        const spieler::Shader::Config shaderConfig
        {
            .Type{ spieler::Shader::Type::Compute },
            .Filepath{ L"assets/shaders/sobel_filter.hlsl" },
            .EntryPoint{ "CS_Main" }
        };

        m_Shader = spieler::Shader::Create(shaderConfig);

        const spieler::ComputePipelineState::Config pipelineStateConfig
        {
            .RootSignature{ &m_RootSignature },
            .ComputeShader{ m_Shader.get() }
        };

        m_PipelineState = spieler::ComputePipelineState(device, pipelineStateConfig);
    }

} // namespace sandbox
