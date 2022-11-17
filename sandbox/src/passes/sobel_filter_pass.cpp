#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <spieler/graphics/renderer.hpp>

namespace sandbox
{

    SobelFilterPass::SobelFilterPass(uint32_t width, uint32_t height)
    {
        InitOutputTexture(width, height);
        InitRootSignature();
        InitPipelineState();
    }

    void SobelFilterPass::Execute(spieler::Texture& input)
    {
        auto& renderer{ spieler::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        auto* dx12GraphicsCommandList{ context.GetDX12GraphicsCommandList() };

        const uint32_t width{ static_cast<uint32_t>(input.GetTextureResource()->GetConfig().Width) };
        const uint32_t height{ input.GetTextureResource()->GetConfig().Height };

        context.SetPipelineState(m_PipelineState);
        context.SetComputeRootSignature(m_RootSignature);

        dx12GraphicsCommandList->SetComputeRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE{ input.GetView<spieler::TextureShaderResourceView>().GetDescriptor().GPU });
        dx12GraphicsCommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_OutputTexture.GetView<spieler::TextureUnorderedAccessView>().GetDescriptor().GPU });

        const uint32_t threadGroupXCount{ width / 16 + 1 };
        const uint32_t threadGroupYCount{ height / 16 + 1 };
        dx12GraphicsCommandList->Dispatch(threadGroupXCount, threadGroupYCount, 1);
    }

    void SobelFilterPass::OnResize(uint32_t width, uint32_t height)
    {
        InitOutputTexture(width, height);
    }

    void SobelFilterPass::InitOutputTexture(uint32_t width, uint32_t height)
    {
        auto& device{ spieler::Renderer::GetInstance().GetDevice() };

        const spieler::TextureResource::Config config
        {
            .Width{ width },
            .Height{ height },
            .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .UsageFlags{ spieler::TextureResource::UsageFlags::UnorderedAccess }
        };

        m_OutputTexture.SetTextureResource(spieler::TextureResource::Create(device, config));
        m_OutputTexture.PushView<spieler::TextureShaderResourceView>(device);
        m_OutputTexture.PushView<spieler::TextureUnorderedAccessView>(device);
    }

    void SobelFilterPass::InitRootSignature()
    {
        auto& device{ spieler::Renderer::GetInstance().GetDevice() };

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

    void SobelFilterPass::InitPipelineState()
    {
        auto& device{ spieler::Renderer::GetInstance().GetDevice() };

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
