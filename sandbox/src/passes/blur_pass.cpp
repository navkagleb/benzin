#include "bootstrap.hpp"

#include "blur_pass.hpp"

#include <third_party/magic_enum/magic_enum.hpp>

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/graphics/resource_barrier.hpp>

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

    BlurPass::BlurPass(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
        InitHorizontalPass(device);
        InitVerticalPass(device);
    }

    void BlurPass::OnResize(spieler::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
    }

    bool BlurPass::Execute(spieler::GraphicsCommandList& graphicsCommandList, spieler::TextureResource& input, const BlurPassExecuteProps& props)
    {
        const uint32_t width{ input.GetConfig().Width };
        const uint32_t height{ input.GetConfig().Height };

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ &input },
            .From{ spieler::ResourceState::RenderTarget },
            .To{ spieler::ResourceState::CopySource }
        });

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::Present },
            .To{ spieler::ResourceState::CopyDestination }
        });

        graphicsCommandList.CopyResource(*m_BlurMaps[0].GetTextureResource(), input);

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::CopyDestination },
            .To{ spieler::ResourceState::GenericRead }
        });

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
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
                graphicsCommandList.SetPipelineState(m_HorizontalPass.PSO);
                graphicsCommandList.SetComputeRootSignature(m_HorizontalPass.RootSignature);

                graphicsCommandList.SetCompute32BitConstants(0, &horizontalBlurRadius, 1, 0);
                graphicsCommandList.SetCompute32BitConstants(0, horizontalWeights.data(), horizontalWeights.size(), 1);

                graphicsCommandList.SetComputeDescriptorTable(1, m_BlurMaps[0].GetView<spieler::TextureShaderResourceView>());
                graphicsCommandList.SetComputeDescriptorTable(2, m_BlurMaps[1].GetView<spieler::TextureUnorderedAccessView>());

                const auto xGroupCount{ width / _internal::g_ThreadPerGroupCount + 1 };
                graphicsCommandList.Dispatch(xGroupCount, height, 1);

                graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[0].GetTextureResource().get() },
                    .From{ spieler::ResourceState::GenericRead },
                    .To{ spieler::ResourceState::UnorderedAccess }
                });

                graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[1].GetTextureResource().get() },
                    .From{ spieler::ResourceState::UnorderedAccess },
                    .To{ spieler::ResourceState::GenericRead }
                });
            }
            
            // Vertical Blur pass
            {
                graphicsCommandList.SetPipelineState(m_VerticalPass.PSO);
                graphicsCommandList.SetComputeRootSignature(m_VerticalPass.RootSignature);

                graphicsCommandList.SetCompute32BitConstants(0, &verticalBlurRadius, 1, 0);
                graphicsCommandList.SetCompute32BitConstants(0, verticalWeights.data(), verticalWeights.size(), 1);

                graphicsCommandList.SetComputeDescriptorTable(1, m_BlurMaps[1].GetView<spieler::TextureShaderResourceView>());
                graphicsCommandList.SetComputeDescriptorTable(2, m_BlurMaps[0].GetView<spieler::TextureUnorderedAccessView>());

                const uint32_t yGroupCount{ height / _internal::g_ThreadPerGroupCount + 1 };
                graphicsCommandList.Dispatch(width, yGroupCount, 1);

                graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[0].GetTextureResource().get() },
                    .From{ spieler::ResourceState::UnorderedAccess },
                    .To{ spieler::ResourceState::GenericRead }
                });

                graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
                {
                    .Resource{ m_BlurMaps[1].GetTextureResource().get() },
                    .From{ spieler::ResourceState::GenericRead },
                    .To{ spieler::ResourceState::UnorderedAccess }
                });
            }
        }

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[0].GetTextureResource().get() },
            .From{ spieler::ResourceState::GenericRead },
            .To{ spieler::ResourceState::Present }
        });

        graphicsCommandList.SetResourceBarrier(spieler::TransitionResourceBarrier
        {
            .Resource{ m_BlurMaps[1].GetTextureResource().get() },
            .From{ spieler::ResourceState::UnorderedAccess },
            .To{ spieler::ResourceState::Present }
        });

        return true;
    }

    void BlurPass::InitTextures(spieler::Device& device, uint32_t width, uint32_t height)
    {
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

    void BlurPass::InitHorizontalPass(spieler::Device& device)
    {
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

    void BlurPass::InitVerticalPass(spieler::Device& device)
    {
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
