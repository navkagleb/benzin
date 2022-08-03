#include "bootstrap.hpp"

#include "sobel_filter_pass.hpp"

#include <spieler/renderer/renderer.hpp>

namespace sandbox
{

    namespace per
    {

        enum class SobelFilter {};

    } // namespace per

    SobelFilterPass::SobelFilterPass(uint32_t width, uint32_t height)
    {
        InitOutputTexture(width, height);
        InitRootSignature();
        InitPipelineState();
    }

    void SobelFilterPass::Execute(spieler::renderer::Texture& input)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        auto* dx12GraphicsCommandList{ context.GetDX12GraphicsCommandList() };

        const uint32_t width{ static_cast<uint32_t>(input.Resource.GetConfig().Width) };
        const uint32_t height{ input.Resource.GetConfig().Height };

        context.SetPipelineState(m_PipelineState);
        context.SetComputeRootSignature(m_RootSignature);

        dx12GraphicsCommandList->SetComputeRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE{ input.Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
        dx12GraphicsCommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_OutputTexture.Views.GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

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
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::TextureResource::Config config
        {
            .Width{ width },
            .Height{ height },
            .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ spieler::renderer::TextureResource::Flags::UnorderedAccess }
        };

        m_OutputTexture.Resource = spieler::renderer::TextureResource{ device, config };

        //m_OutputTexture.GetTexture2DResource().SetDebugName(L"SobelFilterOutput");

        m_OutputTexture.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
        m_OutputTexture.Views.CreateView<spieler::renderer::UnorderedAccessView>(device);
    }

    void SobelFilterPass::InitRootSignature()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::RootParameter::SingleDescriptorTable srvTable
        {
            .Range
            {
                .Type{ spieler::renderer::RootParameter::DescriptorRangeType::ShaderResourceView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        const spieler::renderer::RootParameter::SingleDescriptorTable uavTable
        {
            .Range
            {
                .Type{ spieler::renderer::RootParameter::DescriptorRangeType::UnorderedAccessView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        spieler::renderer::RootSignature::Config rootSignatureConfig{ 2 };
        rootSignatureConfig.RootParameters[0] = srvTable;
        rootSignatureConfig.RootParameters[1] = uavTable;

        m_RootSignature = spieler::renderer::RootSignature{ device, rootSignatureConfig };
    }

    void SobelFilterPass::InitPipelineState()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::ShaderConfig<per::SobelFilter> shaderConfig
        {
            .Filename{ L"assets/shaders/sobel_filter.hlsl" },
            .EntryPoint{ "CS_Main" },
            .Permutation{ spieler::renderer::ShaderPermutation<per::SobelFilter>{} }
        };

        const spieler::renderer::Shader& computeShader{ m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Compute, shaderConfig) };

        const spieler::renderer::ComputePipelineState::Config pipelineStateConfig
        {
            .RootSignature{ &m_RootSignature },
            .ComputeShader{ &computeShader }
        };

        m_PipelineState = spieler::renderer::ComputePipelineState(device, pipelineStateConfig);
    }

} // namespace sandbox
