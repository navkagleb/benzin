#include "bootstrap.hpp"

#include "blur_pass.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/graphics/renderer.hpp>

namespace sandbox
{

    namespace _internal
    {

        constexpr int32_t g_ThreadPerGroupCount{ 256 };
        constexpr int32_t g_MaxBlurRadius{ 5 };

        static std::vector<float> CalcGaussWeights(float sigma)
        {
            const float twoSigma2{ 2.0f * sigma * sigma };
            const auto blurRadius{ static_cast<int32_t>(std::ceil(2.0f * sigma)) };

            SPIELER_ASSERT(blurRadius <= g_MaxBlurRadius);

            std::vector<float> weights;
            weights.reserve(2 * blurRadius + 1);

            float weightSum{ 0.0f };

            for (int32_t i = -blurRadius; i < blurRadius + 1; ++i)
            {
                const auto x{ static_cast<float>(i) };

                weightSum += weights.emplace_back(std::expf(-x * x / twoSigma2));
            }

            for (int i = 0; i < weights.size(); ++i)
            {
                weights[i] /= weightSum;
            }

            return weights;
        }

        static spieler::RootSignature::Config GetUniformPassRootSignatureConfig()
        {
            const spieler::RootParameter::_32BitConstants constants
            {
                .ShaderRegister{ 0 },
                .Count{ 12 } // BlurRadius + 11 Weights
            };

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

            spieler::RootSignature::Config config{ 3 };
            config.RootParameters[0] = constants;
            config.RootParameters[1] = srvTable;
            config.RootParameters[2] = uavTable;
            
            return config;
        }

    } // namespace _internal

    BlurPass::BlurPass(uint32_t width, uint32_t height)
    {
        InitTextures(width, height);
        InitHorizontalPass();
        InitVerticalPass();
    }

    void BlurPass::OnResize(uint32_t width, uint32_t height)
    {
        InitTextures(width, height);
    }

    bool BlurPass::Execute(spieler::TextureResource& input, const BlurPassExecuteProps& props)
    {
        spieler::Renderer& renderer{ spieler::Renderer::GetInstance() };
        spieler::Context& context{ renderer.GetContext() };

        const uint32_t width{ input.GetConfig().Width };
        const uint32_t height{ input.GetConfig().Height };

        auto* dx12CommandList{ context.GetDX12GraphicsCommandList() };

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ &input },
            .From{ spieler::ResourceState::RenderTarget },
            .To{ spieler::ResourceState::CopySource }
        });

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::Present },
            .To{ spieler::ResourceState::CopyDestination }
        });

        dx12CommandList->CopyResource(m_BlurMaps[0].GetTextureResource()->GetDX12Resource(), input.GetDX12Resource());

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::CopyDestination },
            .To{ spieler::ResourceState::GenericRead }
        });

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[1].GetTextureResource().get() },
            .From{ spieler::ResourceState::Present },
            .To{ spieler::ResourceState::UnorderedAccess }
        });

        const std::vector<float> horizontalWeights{ _internal::CalcGaussWeights(props.HorizontalBlurSigma) };
        const auto horizontalBlurRadius{ static_cast<int32_t>(horizontalWeights.size() / 2) };

        const std::vector<float> verticalWeights{ _internal::CalcGaussWeights(props.VerticalBlurSigma) };
        const auto verticalBlurRadius{ static_cast<int32_t>(verticalWeights.size() / 2) };

        for (uint32_t i = 0; i < props.BlurCount; ++i)
        {
            // Horizontal Blur pass
            {
                context.SetPipelineState(m_HorizontalPass.PSO);
                context.SetComputeRootSignature(m_HorizontalPass.RootSignature);

                dx12CommandList->SetComputeRoot32BitConstants(0, 1, &horizontalBlurRadius, 0);
                dx12CommandList->SetComputeRoot32BitConstants(0, static_cast<uint32_t>(horizontalWeights.size()), horizontalWeights.data(), 1);

                dx12CommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].GetView<spieler::TextureShaderResourceView>().GetDescriptor().GPU });
                dx12CommandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].GetView<spieler::TextureUnorderedAccessView>().GetDescriptor().GPU });

                const auto xGroupCount{ width / _internal::g_ThreadPerGroupCount + 1 };
                dx12CommandList->Dispatch(xGroupCount, height, 1);

                context.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[0].GetTextureResource().get() },
                    .From{ spieler::ResourceState::GenericRead },
                    .To{ spieler::ResourceState::UnorderedAccess }
                });

                context.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[1].GetTextureResource().get() },
                    .From{ spieler::ResourceState::UnorderedAccess },
                    .To{ spieler::ResourceState::GenericRead }
                });
            }
            
            // Vertical Blur pass
            {
                context.SetPipelineState(m_VerticalPass.PSO);
                context.SetComputeRootSignature(m_VerticalPass.RootSignature);

                dx12CommandList->SetComputeRoot32BitConstants(0, 1, &verticalBlurRadius, 0);
                dx12CommandList->SetComputeRoot32BitConstants(0, static_cast<uint32_t>(verticalWeights.size()), verticalWeights.data(), 1);

                dx12CommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].GetView<spieler::TextureShaderResourceView>().GetDescriptor().GPU });
                dx12CommandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].GetView<spieler::TextureUnorderedAccessView>().GetDescriptor().GPU });

                const uint32_t yGroupCount{ height / _internal::g_ThreadPerGroupCount + 1 };
                dx12CommandList->Dispatch(width, yGroupCount, 1);

                context.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[0].GetTextureResource().get() },
                    .From{ spieler::ResourceState::UnorderedAccess },
                    .To{ spieler::ResourceState::GenericRead }
                });

                context.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[1].GetTextureResource().get() },
                    .From{ spieler::ResourceState::GenericRead },
                    .To{ spieler::ResourceState::UnorderedAccess }
                });
            }
        }

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::GenericRead },
            .To{ spieler::ResourceState::Present }
        });

        context.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[1].GetTextureResource().get() },
            .From{ spieler::ResourceState::UnorderedAccess },
            .To{ spieler::ResourceState::Present }
        });

        return true;
    }

    void BlurPass::InitTextures(uint32_t width, uint32_t height)
    {
        auto& device{ spieler::Renderer::GetInstance().GetDevice() };

        const spieler::TextureResource::Config config
        {
            .Width{ width },
            .Height{ height },
            .Format{ spieler::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .UsageFlags{ spieler::TextureResource::UsageFlags::UnorderedAccess },
        };

        for (size_t i = 0; i < m_BlurMaps.size(); ++i)
        {
            auto& map{ m_BlurMaps[i] };
            map.SetTextureResource(spieler::TextureResource::Create(device, config));
            map.PushView<spieler::TextureShaderResourceView>(device);
            map.PushView<spieler::TextureUnorderedAccessView>(device);
        }
    }

    void BlurPass::InitHorizontalPass()
    {
        spieler::Renderer& renderer{ spieler::Renderer::GetInstance() };
        spieler::Device& device{ renderer.GetDevice() };

        // Root Signature
        {
            m_HorizontalPass.RootSignature = spieler::RootSignature{ device, _internal::GetUniformPassRootSignatureConfig() };
        }

        // PSO
        {
            const spieler::Shader::Config shaderConfig
            {
                .Type{ spieler::Shader::Type::Compute },
                .Filepath{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_HorizontalBlur" },
                .Defines
                {
                    { "THREAD_PER_GROUP_COUNT", std::to_string(_internal::g_ThreadPerGroupCount)}
                }
            };

            m_ShaderLibrary["horizontal_cs"] = spieler::Shader::Create(shaderConfig);

            const spieler::ComputePipelineState::Config horizontalPSOConfig
            {
                .RootSignature{ &m_HorizontalPass.RootSignature },
                .ComputeShader{ m_ShaderLibrary["horizontal_cs"].get() }
            };

            m_HorizontalPass.PSO = spieler::ComputePipelineState(device, horizontalPSOConfig);
        }
    }

    void BlurPass::InitVerticalPass()
    {
        spieler::Renderer& renderer{ spieler::Renderer::GetInstance() };
        spieler::Device& device{ renderer.GetDevice() };

        // Root Signature
        {
            m_VerticalPass.RootSignature = spieler::RootSignature{ device, _internal::GetUniformPassRootSignatureConfig() };
        }

        // PSO
        {
            const spieler::Shader::Config shaderConfig
            {
                .Type{ spieler::Shader::Type::Compute },
                .Filepath{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_VerticalBlur" },
                .Defines
                {
                    { "THREAD_PER_GROUP_COUNT", std::to_string(_internal::g_ThreadPerGroupCount)}
                }
            };

            m_ShaderLibrary["vertical_cs"] = spieler::Shader::Create(shaderConfig);

            const spieler::ComputePipelineState::Config verticalPSOConfig
            {
                .RootSignature{ &m_VerticalPass.RootSignature },
                .ComputeShader{ m_ShaderLibrary["vertical_cs"].get() }
            };

            m_VerticalPass.PSO = spieler::ComputePipelineState(device, verticalPSOConfig);
        }
    }

} // namespace sandbox
