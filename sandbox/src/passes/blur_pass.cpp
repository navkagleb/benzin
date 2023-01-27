#include "bootstrap.hpp"

#include "blur_pass.hpp"

#include <benzin/graphics/resource_view_builder.hpp>

namespace sandbox
{

    namespace
    {

        constexpr int32_t g_ThreadPerGroupCount{ 256 };
        constexpr int32_t g_MaxBlurRadius{ 5 };

        std::vector<float> CalcGaussWeights(float sigma)
        {
            const float twoSigma2{ 2.0f * sigma * sigma };
            const auto blurRadius{ static_cast<int32_t>(std::ceil(2.0f * sigma)) };

            BENZIN_ASSERT(blurRadius <= g_MaxBlurRadius);

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

    } // anonymous namespace

    BlurPass::BlurPass(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
        InitRootSignature(device);
        InitPSOs(device);
    }

    void BlurPass::OnExecute(benzin::GraphicsCommandList& graphicsCommandList, benzin::TextureResource& input, const BlurPassExecuteProps& props)
    {
        const uint32_t width = input.GetConfig().Width;
        const uint32_t height = input.GetConfig().Height;

        graphicsCommandList.SetResourceBarrier(*m_BlurMaps[0], benzin::Resource::State::CopyDestination);
        graphicsCommandList.CopyResource(*m_BlurMaps[0], input);

        graphicsCommandList.SetResourceBarrier(*m_BlurMaps[0], benzin::Resource::State::GenericRead);
        graphicsCommandList.SetResourceBarrier(*m_BlurMaps[1], benzin::Resource::State::UnorderedAccess);

        const std::vector<float> horizontalWeights = CalcGaussWeights(props.HorizontalBlurSigma);
        const uint32_t horizontalBlurRadius = horizontalWeights.size() / 2;

        const std::vector<float> verticalWeights = CalcGaussWeights(props.VerticalBlurSigma);
        const uint32_t verticalBlurRadius = verticalWeights.size() / 2;

        for (uint32_t i = 0; i < props.BlurCount; ++i)
        {
            // Horizontal Blur pass
            {
                graphicsCommandList.SetPipelineState(m_HorizontalPSO);
                graphicsCommandList.SetComputeRootSignature(m_RootSignature);

                graphicsCommandList.SetCompute32BitConstants(0, &horizontalBlurRadius, 1, 0);
                graphicsCommandList.SetCompute32BitConstants(0, horizontalWeights.data(), horizontalWeights.size(), 1);

                graphicsCommandList.SetComputeDescriptorTable(1, m_BlurMaps[0]->GetShaderResourceView());
                graphicsCommandList.SetComputeDescriptorTable(2, m_BlurMaps[1]->GetUnorderedAccessView());

                const uint32_t xGroupCount = width / g_ThreadPerGroupCount + 1;
                graphicsCommandList.Dispatch(xGroupCount, height, 1);

                graphicsCommandList.SetResourceBarrier(*m_BlurMaps[0], benzin::Resource::State::UnorderedAccess);
                graphicsCommandList.SetResourceBarrier(*m_BlurMaps[1], benzin::Resource::State::GenericRead);
            }
            
            // Vertical Blur pass
            {
                graphicsCommandList.SetPipelineState(m_VerticalPSO);
                graphicsCommandList.SetComputeRootSignature(m_RootSignature);

                graphicsCommandList.SetCompute32BitConstants(0, &verticalBlurRadius, 1, 0);
                graphicsCommandList.SetCompute32BitConstants(0, verticalWeights.data(), verticalWeights.size(), 1);

                graphicsCommandList.SetComputeDescriptorTable(1, m_BlurMaps[1]->GetShaderResourceView());
                graphicsCommandList.SetComputeDescriptorTable(2, m_BlurMaps[0]->GetUnorderedAccessView());

                const uint32_t yGroupCount = height / g_ThreadPerGroupCount + 1;
                graphicsCommandList.Dispatch(width, yGroupCount, 1);

                graphicsCommandList.SetResourceBarrier(*m_BlurMaps[0], benzin::Resource::State::GenericRead);
                graphicsCommandList.SetResourceBarrier(*m_BlurMaps[1], benzin::Resource::State::UnorderedAccess);
            }
        }

        graphicsCommandList.SetResourceBarrier(*m_BlurMaps[0], benzin::Resource::State::Present);
        graphicsCommandList.SetResourceBarrier(*m_BlurMaps[1], benzin::Resource::State::Present);
    }

    void BlurPass::OnResize(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
    }

    void BlurPass::InitTextures(benzin::Device& device, uint32_t width, uint32_t height)
    {
        const benzin::TextureResource::Config config
        {
            .Type{ benzin::TextureResource::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .Format{ benzin::GraphicsFormat::R8G8B8A8UnsignedNorm },
            .Flags{ benzin::TextureResource::Flags::BindAsUnorderedAccess },
        };

        for (size_t i = 0; i < m_BlurMaps.size(); ++i)
        {
            auto& blurMap = m_BlurMaps[i];
            blurMap = device.CreateTextureResource(config);
            blurMap->PushShaderResourceView(device.GetResourceViewBuilder().CreateShaderResourceView(*blurMap));
            blurMap->PushUnorderedAccessView(device.GetResourceViewBuilder().CreateUnorderedAccessView(*blurMap));
            blurMap->SetName("BlurMap" + std::to_string(i));
        }
    }

    void BlurPass::InitRootSignature(benzin::Device& device)
    {
        const benzin::RootParameter::_32BitConstants constants
        {
            .ShaderRegister{ 0 },
            .Count{ 12 } // BlurRadius + 11 Weights
        };

        const benzin::RootParameter::SingleDescriptorTable srvTable
        {
            .Range
            {
                .Type{ benzin::RootParameter::DescriptorRangeType::ShaderResourceView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        const benzin::RootParameter::SingleDescriptorTable uavTable
        {
            .Range
            {
                .Type{ benzin::RootParameter::DescriptorRangeType::UnorderedAccessView },
                .DescriptorCount{ 1 },
                .BaseShaderRegister{ 0 }
            }
        };

        benzin::RootSignature::Config config{ 3 };
        config.RootParameters[0] = constants;
        config.RootParameters[1] = srvTable;
        config.RootParameters[2] = uavTable;

        m_RootSignature = benzin::RootSignature{ device, config };
    }

    void BlurPass::InitPSOs(benzin::Device& device)
    {
        // Horizontal
        {
            const benzin::Shader::Config shaderConfig
            {
                .Type{ benzin::Shader::Type::Compute },
                .Filepath{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_HorizontalBlur" },
                .Defines
                {
                    { "THREAD_PER_GROUP_COUNT", std::to_string(g_ThreadPerGroupCount) }
                }
            };

            m_ShaderLibrary["horizontal_cs"] = benzin::Shader::Create(shaderConfig);

            const benzin::ComputePipelineState::Config horizontalPSOConfig
            {
                .RootSignature{ &m_RootSignature },
                .ComputeShader{ m_ShaderLibrary["horizontal_cs"].get() }
            };

            m_HorizontalPSO = benzin::ComputePipelineState{ device, horizontalPSOConfig };
        }

        // Vertical
        {
            const benzin::Shader::Config shaderConfig
            {
                .Type{ benzin::Shader::Type::Compute },
                .Filepath{ L"assets/shaders/blur.hlsl" },
                .EntryPoint{ "CS_VerticalBlur" },
                .Defines
                {
                    { "THREAD_PER_GROUP_COUNT", std::to_string(g_ThreadPerGroupCount) }
                }
            };

            m_ShaderLibrary["vertical_cs"] = benzin::Shader::Create(shaderConfig);

            const benzin::ComputePipelineState::Config verticalPSOConfig
            {
                .RootSignature{ &m_RootSignature },
                .ComputeShader{ m_ShaderLibrary["vertical_cs"].get() }
            };

            m_VerticalPSO = benzin::ComputePipelineState{ device, verticalPSOConfig };
        }
    }

} // namespace sandbox
