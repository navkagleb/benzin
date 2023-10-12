#include "bootstrap.hpp"
#include "blur_pass.hpp"

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/mapped_data.hpp>

#if 0

namespace sandbox
{

    namespace
    {

        constexpr int32_t g_ThreadPerGroupCount = 256;
        constexpr int32_t g_MaxBlurRadius = 5;
        constexpr size_t g_MaxWeightCount = 11;

        struct __declspec(align(16)) Settings
        {
            int32_t BlurRadius{ 0 };
            std::array<float, g_MaxWeightCount> Weights;
        };

        std::array<float, g_MaxWeightCount> CalcGaussWeights(int32_t blurRadius, float squareSigma2)
        {
            const int32_t weightCount = blurRadius * 2 + 1;
            std::array<float, g_MaxWeightCount> weights;
            weights.fill(0);

            float weightSum = 0.0f;

            for (int32_t i = -blurRadius; i < blurRadius + 1; ++i)
            {
                const auto weightIndex = i + blurRadius;

                const auto x = static_cast<float>(i);
                weights[weightIndex] = std::expf(-x * x / squareSigma2);

                weightSum += weights[weightIndex];
            }

            for (int32_t i = 0; i < weightCount; ++i)
            {
                weights[i] /= weightSum;
            }

            return weights;
        };

        Settings CalcSettingsForSigma(float sigma)
        {
            const int32_t blurRadius = static_cast<int32_t>(std::ceil(2.0f * sigma));
            BenzinAssert(blurRadius <= g_MaxBlurRadius);

            return Settings
            {
                .BlurRadius{ blurRadius },
                .Weights{ CalcGaussWeights(blurRadius, sigma * sigma * 2.0f) }
            };
        }

        void UpdateSettingsBuffer(benzin::BufferResource& buffer, const Settings& settings)
        {
            benzin::MappedData mappedBuffer{ buffer };
            mappedBuffer.Write(&settings, sizeof(settings));
        }

    } // anonymous namespace

    BlurPass::BlurPass(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitPasses(device);
        InitTextures(device, width, height);
    }

    void BlurPass::OnExecute(benzin::GraphicsCommandList& commandList, benzin::Texture& input, const ExecuteArgs& args)
    {
        UpdateSettingsBuffer(*m_HorizontalPass.SettingsBuffer, CalcSettingsForSigma(args.HorizontalBlurSigma));
        UpdateSettingsBuffer(*m_VerticalPass.SettingsBuffer, CalcSettingsForSigma(args.VerticalBlurSigma));

        const uint32_t width = input.GetConfig().Width;
        const uint32_t height = input.GetConfig().Height;

        commandList.SetResourceBarrier(*m_BlurMaps[0], benzin::ResourceState::CopyDestination);
        commandList.CopyResource(*m_BlurMaps[0], input);

        commandList.SetResourceBarrier(*m_BlurMaps[0], benzin::ResourceState::GenericRead);
        commandList.SetResourceBarrier(*m_BlurMaps[1], benzin::ResourceState::UnorderedAccess);

        for (uint32_t i = 0; i < args.BlurCount; ++i)
        {
            {
                const uint32_t threadGroupCountX = benzin::AlignThreadGroupCount(width, g_ThreadPerGroupCount);
                const uint32_t threadGroupCountY = height;
                ExecuteDirectionPass(commandList, *m_BlurMaps[0], *m_BlurMaps[1], m_HorizontalPass, threadGroupCountX, threadGroupCountY);
            }
            
            {
                const uint32_t threadGroupCountX = width;
                const uint32_t threadGroupCountY = benzin::AlignThreadGroupCount(height, g_ThreadPerGroupCount);
                ExecuteDirectionPass(commandList, *m_BlurMaps[1], *m_BlurMaps[0], m_VerticalPass, threadGroupCountX, threadGroupCountY);
            }
        }

        commandList.SetResourceBarrier(*m_BlurMaps[0], benzin::ResourceState::Present);
        commandList.SetResourceBarrier(*m_BlurMaps[1], benzin::ResourceState::Present);
    }

    void BlurPass::OnResize(benzin::Device& device, uint32_t width, uint32_t height)
    {
        InitTextures(device, width, height);
    }

    void BlurPass::InitPasses(benzin::Device& device)
    {
        static const auto initPass = [&](DirectionPass& pass, const std::string& passName)
        {
            {
                const benzin::BufferConfig config
                {
                    .ElementSize{ sizeof(Settings) },
                    .ElementCount{ 1 },
                    .Flags{ benzin::BufferFlag::ConstantBuffer }
                };

                pass.SettingsBuffer = std::make_shared<benzin::BufferResource>(device, config);
                pass.SettingsBuffer->SetDebugName(passName + "BlurSettings");
                pass.SettingsBuffer->PushConstantBufferView();
            }
            
            {
                const std::string shaderEntryPoint = "CS_" + passName + "Blur";

                const benzin::ComputePipelineState::Config config
                {
                    .ComputeShader{ "blur.hlsl", shaderEntryPoint }
                };

                pass.PipelineState = std::make_unique<benzin::ComputePipelineState>(device, config);
                pass.PipelineState->SetDebugName(passName + "Blur");
            }
        };

        initPass(m_HorizontalPass, "Horizontal");
        initPass(m_VerticalPass, "Vertical");
    }

    void BlurPass::InitTextures(benzin::Device& device, uint32_t width, uint32_t height)
    {
        const benzin::Texture::Config config
        {
            .Type{ benzin::Texture::Type::Texture2D },
            .Width{ width },
            .Height{ height },
            .Format{ benzin::GraphicsFormat::RGBA8Unorm },
            .Flags{ benzin::Texture::Flags::BindAsUnorderedAccess },
        };

        for (size_t i = 0; i < m_BlurMaps.size(); ++i)
        {
            auto& blurMap = m_BlurMaps[i];
            blurMap = std::make_shared<benzin::Texture>(device, config);
            blurMap->SetDebugName("BlurMap" + std::to_string(0));
            blurMap->PushShaderResourceView();
            blurMap->PushUnorderedAccessView();
        }
    }
    
    void BlurPass::ExecuteDirectionPass(
        benzin::GraphicsCommandList& commandList,
        benzin::Texture& input,
        benzin::Texture& output,
        const DirectionPass& pass,
        uint32_t threadGroupCountX,
        uint32_t threadGroupCountY
    )
    {
        enum class RootConstant : uint32_t
        {
            SettingsBufferIndex,
            InputTextureIndex,
            OutputTextureIndex
        };

        commandList.SetPipelineState(*pass.PipelineState);
        commandList.SetRootConstantBufferC(RootConstant::SettingsBufferIndex, pass.SettingsBuffer->GetConstantBufferView());
        commandList.SetRootShaderResourceC(RootConstant::InputTextureIndex, input.GetShaderResourceView());
        commandList.SetRootUnorderedAccessC(RootConstant::OutputTextureIndex, output.GetUnorderedAccessView());

        BenzinAssert(threadGroupCountX && threadGroupCountY);
        commandList.Dispatch(threadGroupCountX, threadGroupCountY, 1);

        commandList.SetResourceBarrier(input, benzin::ResourceState::UnorderedAccess);
        commandList.SetResourceBarrier(output, benzin::ResourceState::GenericRead);
    }

} // namespace sandbox

#endif
