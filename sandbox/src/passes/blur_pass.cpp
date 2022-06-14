#include "bootstrap.hpp"

#include "blur_pass.hpp"

#include <spieler/system/event_dispatcher.hpp>

#include <spieler/renderer/renderer.hpp>

namespace sandbox
{

    namespace _internal
    {

        constexpr int32_t g_MaxBlurRadius{ 5 };

        static std::vector<float> CalcGaussWeights(float sigma)
        {
            const float twoSigma2{ 2.0f * sigma * sigma };
            const auto blurRadius{ static_cast<int32_t>(std::ceil(2.0f * sigma)) };

            SPIELER_ASSERT(blurRadius <= g_MaxBlurRadius);

            std::vector<float> weights;
            weights.reserve(2 * blurRadius + 1);

            float weightSum{ 0.0f };

            for (int32_t i = -blurRadius; i < blurRadius  + 1; ++i)
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

    } // namespace _internal

    namespace per
    {

        enum class Horizontal {};

        enum class Vertical {};

    } // namespace per

    BlurPass::BlurPass(uint32_t width, uint32_t height)
    {
        InitTextures(width, height);
        InitRootSignature();
        InitPipelineStates();
    }

    void BlurPass::OnResize(uint32_t width, uint32_t height)
    {
        InitTextures(width, height);
    }

    bool BlurPass::Execute(spieler::renderer::Texture2DResource& input, uint32_t width, uint32_t height, uint32_t blurCount)
    {
        spieler::renderer::Renderer& renderer{ spieler::renderer::Renderer::GetInstance() };
        spieler::renderer::Context& context{ renderer.GetContext() };

        auto& commandList{ context.GetNativeCommandList() };

        const std::vector<float> weights{ _internal::CalcGaussWeights(2.5f) };
        const auto blurRadius{ static_cast<int32_t>(weights.size() / 2) };

        commandList->SetComputeRootSignature(static_cast<ID3D12RootSignature*>(m_RootSignature));
        commandList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0);
        commandList->SetComputeRoot32BitConstants(0, static_cast<uint32_t>(weights.size()), weights.data(), 1);

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &input,
            .From = spieler::renderer::ResourceState::RenderTarget,
            .To = spieler::renderer::ResourceState::CopySource
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &m_BlurMaps[0].GetTexture2DResource(),
            .From = spieler::renderer::ResourceState::Present,
            .To = spieler::renderer::ResourceState::CopyDestination
        });

        commandList->CopyResource(m_BlurMaps[0].GetTexture2DResource().GetResource().Get(), input);

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &m_BlurMaps[0].GetTexture2DResource(),
            .From = spieler::renderer::ResourceState::CopyDestination,
            .To = spieler::renderer::ResourceState::GenericRead
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &m_BlurMaps[1].GetTexture2DResource(),
            .From = spieler::renderer::ResourceState::Present,
            .To = spieler::renderer::ResourceState::UnorderedAccess
        });

        for (uint32_t i = 0; i < blurCount; ++i)
        {
            // Horizontal Blur pass
            {
                commandList->SetPipelineState(static_cast<ID3D12PipelineState*>(m_HorizontalBlurPSO));

                commandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
                commandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

                // How many groups do we need to dispatch to cover a row of pixels, where each
                // group covers 256 pixels (the 256 is defined in the ComputeShader).
                const auto xGroupCount{ static_cast<uint32_t>(std::ceilf(width / 256.0f)) };
                commandList->Dispatch(xGroupCount, height, 1);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &m_BlurMaps[0].GetTexture2DResource(),
                    .From = spieler::renderer::ResourceState::GenericRead,
                    .To = spieler::renderer::ResourceState::UnorderedAccess
                });

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &m_BlurMaps[1].GetTexture2DResource(),
                    .From = spieler::renderer::ResourceState::UnorderedAccess,
                    .To = spieler::renderer::ResourceState::GenericRead
                });
            }
            
            // Vertical Blur pass
            {
                commandList->SetPipelineState(static_cast<ID3D12PipelineState*>(m_VerticalBlurPSO));

                commandList->SetComputeRootDescriptorTable(1, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[1].GetView<spieler::renderer::ShaderResourceView>().GetDescriptor().GPU });
                commandList->SetComputeRootDescriptorTable(2, D3D12_GPU_DESCRIPTOR_HANDLE{ m_BlurMaps[0].GetView<spieler::renderer::UnorderedAccessView>().GetDescriptor().GPU });

                // How many groups do we need to dispatch to cover a column of pixels, where each
                // group covers 256 pixels (the 256 is defined in the ComputeShader).
                const auto yGroupCount{ static_cast<uint32_t>(std::ceilf(height / 256.0f)) };
                commandList->Dispatch(width, yGroupCount, 1);

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &m_BlurMaps[0].GetTexture2DResource(),
                    .From = spieler::renderer::ResourceState::UnorderedAccess,
                    .To = spieler::renderer::ResourceState::GenericRead
                });

                context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
                {
                    .Resource = &m_BlurMaps[1].GetTexture2DResource(),
                    .From = spieler::renderer::ResourceState::GenericRead,
                    .To = spieler::renderer::ResourceState::UnorderedAccess
                });
            }
        }

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &m_BlurMaps[0].GetTexture2DResource(),
            .From = spieler::renderer::ResourceState::GenericRead,
            .To = spieler::renderer::ResourceState::Present
        });

        context.SetResourceBarrier(spieler::renderer::TransitionResourceBarrier
        {
            .Resource = &m_BlurMaps[1].GetTexture2DResource(),
            .From = spieler::renderer::ResourceState::UnorderedAccess,
            .To = spieler::renderer::ResourceState::Present
        });

        return true;
    }

    void BlurPass::InitTextures(uint32_t width, uint32_t height)
    {
        auto& device{ spieler::renderer::Renderer::GetInstance().GetDevice() };

        const spieler::renderer::Texture2DConfig textureConfig
        {
            .Width = width,
            .Height = height,
            .Format = spieler::renderer::GraphicsFormat::R8G8B8A8UnsignedNorm,
            .Flags = spieler::renderer::Texture2DFlags::UnorderedAccess,
        };

        for (size_t i = 0; i < m_BlurMaps.size(); ++i)
        {
            auto& map{ m_BlurMaps[i] };

            SPIELER_ASSERT(device.CreateTexture(textureConfig, map.GetTexture2DResource()));

            map.SetView<spieler::renderer::ShaderResourceView>(device);
            map.SetView<spieler::renderer::UnorderedAccessView>(device);

            map.GetTexture2DResource().SetDebugName(L"BlurMap" + std::to_wstring(i));
        }
    }

    void BlurPass::InitRootSignature()
    {
        const spieler::renderer::RootParameter constants
        {
            .Type = spieler::renderer::RootParameterType::_32BitConstants,
            .Child = spieler::renderer::RootConstants
            {
                .ShaderRegister = 0,
                .Count = 12
            }
        };

        const spieler::renderer::RootParameter srvTable
        {
            .Type = spieler::renderer::RootParameterType::DescriptorTable,
            .Child = spieler::renderer::RootDescriptorTable
            {
                {
                    spieler::renderer::DescriptorRange
                    {
                        .Type = spieler::renderer::DescriptorRangeType::SRV,
                        .DescriptorCount = 1,
                        .BaseShaderRegister = 0
                    }
                }
            }
        };

        const spieler::renderer::RootParameter uavTable
        {
            .Type = spieler::renderer::RootParameterType::DescriptorTable,
            .Child = spieler::renderer::RootDescriptorTable
            {
                {
                    spieler::renderer::DescriptorRange
                    {
                        .Type = spieler::renderer::DescriptorRangeType::UAV,
                        .DescriptorCount = 1,
                        .BaseShaderRegister = 0
                    }
                }
            }
        };

        std::vector<spieler::renderer::RootParameter> rootParameters{ constants, srvTable, uavTable };

        SPIELER_ASSERT(m_RootSignature.Init(rootParameters));
    }

    void BlurPass::InitPipelineStates()
    {
        spieler::renderer::Renderer& renderer{ spieler::renderer::Renderer::GetInstance() };
        spieler::renderer::Device& device{ renderer.GetDevice() };

        // Horizontal PSO
        {
            const spieler::renderer::ShaderConfig<per::Horizontal> config
            {
                .Filename = L"assets/shaders/blur.hlsl",
                .EntryPoint = "CS_HorizontalBlur",
                .Permutation = spieler::renderer::ShaderPermutation<per::Horizontal>{}
            };

            const spieler::renderer::Shader& horizontalBlurShader{ m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Compute, config) };

            const spieler::renderer::ComputePipelineStateConfig horizontalPSOConfig
            {
                .RootSignature = &m_RootSignature,
                .ComputeShader = &horizontalBlurShader
            };

            m_HorizontalBlurPSO = spieler::renderer::ComputePipelineState(device, horizontalPSOConfig);
        }

        // Vertical PSO
        {
            const spieler::renderer::ShaderConfig<per::Vertical> config
            {
                .Filename = L"assets/shaders/blur.hlsl",
                .EntryPoint = "CS_VerticalBlur",
                .Permutation = spieler::renderer::ShaderPermutation<per::Vertical>{}
            };

            const spieler::renderer::Shader& verticalBlurShader{ m_ShaderLibrary.CreateShader(spieler::renderer::ShaderType::Compute, config) };

            const spieler::renderer::ComputePipelineStateConfig verticalPSOConfig
            {
                .RootSignature = &m_RootSignature,
                .ComputeShader = &verticalBlurShader
            };

            m_VerticalBlurPSO = spieler::renderer::ComputePipelineState(device, verticalPSOConfig);
        }
    }

} // namespace sandbox