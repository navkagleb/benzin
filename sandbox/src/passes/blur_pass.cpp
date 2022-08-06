#include "bootstrap.hpp"

#include "blur_pass.hpp"

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>

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

        static spieler::renderer::RootSignature::Config GetUniformPassRootSignatureConfig()
        {
            const spieler::renderer::RootParameter::_32BitConstants constants
            {
                .ShaderRegister{ 0 },
                .Count{ 12 } // BlurRadius + 11 Weights
            };

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

            spieler::renderer::RootSignature::Config config{ 3 };
            config.RootParameters[0] = constants;
            config.RootParameters[1] = srvTable;
            config.RootParameters[2] = uavTable;
            
            return config;
        }

    } // namespace _internal

    namespace per
    {

        enum class Horizontal
        {
            THREAD_PER_GROUP_COUNT,
        };

        enum class Vertical
        {
            THREAD_PER_GROUP_COUNT,
        };

    } // namespace per

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

    bool BlurPass::Execute(spieler::renderer::TextureResource& input, const BlurPassExecuteProps& props)
    {
        spieler::renderer::Renderer& renderer{ spieler::renderer::Renderer::GetInstance() };
        spieler::renderer::Context& context{ renderer.GetContext() };

        const uint32_t width{ input.GetConfig().Width };
        const uint32_t height{ input.GetConfig().Height };

        auto* dx12CommandList{ context.GetDX12GraphicsCommandList() };

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &input },
            .From{ spieler::renderer::ResourceState::RenderTarget },
            .To{ spieler::renderer::ResourceState::CopySource }
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &m_BlurMaps[0].Resource },
            .From{ spieler::renderer::ResourceState::Present },
            .To{ spieler::renderer::ResourceState::CopyDestination }
        });

        dx12CommandList->CopyResource(m_BlurMaps[0].Resource.GetDX12Resource(), input.GetDX12Resource());

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &m_BlurMaps[0].Resource },
            .From{ spieler::renderer::ResourceState::CopyDestination },
            .To{ spieler::renderer::ResourceState::GenericRead }
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &m_BlurMaps[1].Resource },
            .From{ spieler::renderer::ResourceState::Present },
            .To{ spieler::renderer::ResourceState::UnorderedAccess }
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

                dx12CommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
                dx12CommandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].Views.GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

                const auto xGroupCount{ width / _internal::g_ThreadPerGroupCount + 1 };
                dx12CommandList->Dispatch(xGroupCount, height, 1);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ &m_BlurMaps[0].Resource },
                    .From{ spieler::renderer::ResourceState::GenericRead },
                    .To{ spieler::renderer::ResourceState::UnorderedAccess }
                });

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ &m_BlurMaps[1].Resource },
                    .From{ spieler::renderer::ResourceState::UnorderedAccess },
                    .To{ spieler::renderer::ResourceState::GenericRead }
                });
            }
            
            // Vertical Blur pass
            {
                context.SetPipelineState(m_VerticalPass.PSO);
                context.SetComputeRootSignature(m_VerticalPass.RootSignature);

                dx12CommandList->SetComputeRoot32BitConstants(0, 1, &verticalBlurRadius, 0);
                dx12CommandList->SetComputeRoot32BitConstants(0, static_cast<uint32_t>(verticalWeights.size()), verticalWeights.data(), 1);

                dx12CommandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].Views.GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
                dx12CommandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].Views.GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

                const uint32_t yGroupCount{ height / _internal::g_ThreadPerGroupCount + 1 };
                dx12CommandList->Dispatch(width, yGroupCount, 1);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ &m_BlurMaps[0].Resource },
                    .From{ spieler::renderer::ResourceState::UnorderedAccess },
                    .To{ spieler::renderer::ResourceState::GenericRead }
                });

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource{ &m_BlurMaps[1].Resource },
                    .From{ spieler::renderer::ResourceState::GenericRead },
                    .To{ spieler::renderer::ResourceState::UnorderedAccess }
                });
            }
        }

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &m_BlurMaps[0].Resource },
            .From{ spieler::renderer::ResourceState::GenericRead },
            .To{ spieler::renderer::ResourceState::Present }
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource{ &m_BlurMaps[1].Resource },
            .From{ spieler::renderer::ResourceState::UnorderedAccess },
            .To{ spieler::renderer::ResourceState::Present }
        });

        return true;
    }

    void BlurPass::InitTextures(uint32_t width, uint32_t height)
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::TextureResource::Config config
        {
            .Width{ width },
            .Height{ height },
            .Format{ spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ spieler::renderer::TextureResource::Flags::UnorderedAccess },
        };

        for (size_t i = 0; i < m_BlurMaps.size(); ++i)
        {
            auto& map{ m_BlurMaps[i] };

            map.Resource = spieler::renderer::TextureResource{ device, config };

            map.Views.Clear();
            map.Views.CreateView<spieler::renderer::ShaderResourceView>(device);
            map.Views.CreateView<spieler::renderer::UnorderedAccessView>(device);

            //map.GetTexture2DResource().SetDebugName(L"BlurMap" + std::to_wstring(i));
        }
    }

    void BlurPass::InitHorizontalPass()
    {
        spieler::renderer::Renderer& renderer{ spieler::renderer::Renderer::GetInstance() };
        spieler::renderer::Device& device{ renderer.GetDevice() };

        // Root Signature
        {
            m_HorizontalPass.RootSignature = spieler::renderer::RootSignature{ device, _internal::GetUniformPassRootSignatureConfig() };
        }

        // PSO
        {
            spieler::renderer::ShaderPermutation<per::Horizontal> horizontalPermutation;
            horizontalPermutation.Set<per::Horizontal::THREAD_PER_GROUP_COUNT>(_internal::g_ThreadPerGroupCount);

            const spieler::renderer::ShaderConfig<per::Horizontal> config
            {
                .Filename{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_HorizontalBlur" },
                .Permutation{ horizontalPermutation }
            };

            const spieler::renderer::Shader& horizontalBlurShader{ m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Compute, config) };

            const spieler::renderer::ComputePipelineState::Config horizontalPSOConfig
            {
                .RootSignature{ &m_HorizontalPass.RootSignature },
                .ComputeShader{ &horizontalBlurShader }
            };

            m_HorizontalPass.PSO = spieler::renderer::ComputePipelineState(device, horizontalPSOConfig);
        }
    }

    void BlurPass::InitVerticalPass()
    {
        spieler::renderer::Renderer& renderer{ spieler::renderer::Renderer::GetInstance() };
        spieler::renderer::Device& device{ renderer.GetDevice() };

        // Root Signature
        {
            m_VerticalPass.RootSignature = spieler::renderer::RootSignature{ device, _internal::GetUniformPassRootSignatureConfig() };
        }

        // PSO
        {
            spieler::renderer::ShaderPermutation<per::Vertical> verticalPermutation;
            verticalPermutation.Set<per::Vertical::THREAD_PER_GROUP_COUNT>(_internal::g_ThreadPerGroupCount);

            const spieler::renderer::ShaderConfig<per::Vertical> config
            {
                .Filename{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_VerticalBlur" },
                .Permutation{ verticalPermutation }
            };

            const spieler::renderer::Shader& verticalBlurShader{ m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Compute, config) };

            const spieler::renderer::ComputePipelineState::Config verticalPSOConfig
            {
                .RootSignature{ &m_VerticalPass.RootSignature },
                .ComputeShader{ &verticalBlurShader }
            };

            m_VerticalPass.PSO = spieler::renderer::ComputePipelineState(device, verticalPSOConfig);
        }
    }

} // namespace sandbox