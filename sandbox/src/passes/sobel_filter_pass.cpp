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

    void SobelFilterPass::Execute(spieler::renderer::Texture2D& input)
    {
        auto& renderer{ spieler::renderer::Renderer::GetInstance() };
        auto& device{ renderer.GetDevice() };
        auto& context{ renderer.GetContext() };

        auto& nativeCommandList{ context.GetNativeCommandList() };

        const uint32_t width{ static_cast<uint32_t>(input.GetTexture2DResource().GetConfig().Width) };
        const uint32_t height{ input.GetTexture2DResource().GetConfig().Height };

        context.SetPipelineState(m_PipelineState);
        context.SetComputeRootSignature(m_RootSignature);

        nativeCommandList->SetComputeRootDescriptorTable(0, D3D12_GPU_DESCRIPTOR_HANDLE{ input.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
        nativeCommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_OutputTexture.GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

        const uint32_t threadGroupXCount{ width / 16 + 1 };
        const uint32_t threadGroupYCount{ height / 16 + 1 };
        nativeCommandList->Dispatch(threadGroupXCount, threadGroupYCount, 1);
    }

    void SobelFilterPass::OnResize(uint32_t width, uint32_t height)
    {
        InitOutputTexture(width, height);
    }

    void SobelFilterPass::InitOutputTexture(uint32_t width, uint32_t height)
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::Texture2DConfig outputTextureConfig
        {
            .Width{ static_cast<uint64_t>(width) },
            .Height{ height },
            .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ spieler::renderer::Texture2DFlags::UnorderedAccess }
        };

        SPIELER_ASSERT(device.CreateTexture(outputTextureConfig, m_OutputTexture.GetTexture2DResource()));

        m_OutputTexture.GetTexture2DResource().SetDebugName(L"SobelFilterOutput");

        m_OutputTexture.SetView<spieler::renderer::ShaderResourceView>(device);
        m_OutputTexture.SetView<spieler::renderer::UnorderedAccessView>(device);
    }

    void SobelFilterPass::InitRootSignature()
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        std::vector<spieler::renderer::RootParameter> rootParameters;

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

        rootParameters.emplace_back(srvTable);
        rootParameters.emplace_back(uavTable);

        m_RootSignature = spieler::renderer::RootSignature{ device, rootParameters };
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

        const spieler::renderer::ComputePipelineStateConfig pipelineStateConfig
        {
            .RootSignature{ &m_RootSignature },
            .ComputeShader{ &computeShader }
        };

        m_PipelineState = spieler::renderer::ComputePipelineState(device, pipelineStateConfig);
    }

} // namespace sandbox
