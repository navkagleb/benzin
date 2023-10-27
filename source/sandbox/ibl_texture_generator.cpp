#include "bootstrap.hpp"
#include "ibl_texture_generator.hpp"

#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/texture.hpp>

namespace sandbox
{

    IBLTextureGenerator::IBLTextureGenerator(benzin::Device& device)
        : m_Device{ device }
    {
        m_IrradiancePipelineState = std::make_unique<benzin::PipelineState>(m_Device, benzin::ComputePipelineStateCreation
        {
            .DebugName = "IrradianceGenerator",
            .ComputeShader{ "ibl_texture.hlsl", "CS_Main" },
        });
    }

    benzin::Texture* IBLTextureGenerator::GenerateIrradianceTexture(const benzin::Texture& cubeTexture) const
    {
        const uint32_t textureSize = 64;
        const uint32_t textureArraySize = 6;

        auto* irradianceTexture = new benzin::Texture
        {
            m_Device,
            benzin::TextureCreation
            {
                .DebugName = "Irradiance",
                .Type = benzin::TextureType::Texture2D,
                .IsCubeMap = true,
                .Format = benzin::GraphicsFormat::RGBA32Float,
                .Width = textureSize,
                .Height = textureSize,
                .ArraySize = textureArraySize,
                .MipCount = 1,
                .Flags = benzin::TextureFlag::AllowUnorderedAccess,
                .IsNeedUnorderedAccessView = true,
            },
        };

        {
            enum class RootConstant : uint32_t
            {
                InputCubeTextureIndex,
                OutIrradianceTextureIndex,
            };

            auto& computeCommandQueue = m_Device.GetComputeCommandQueue();
            BenzinFlushCommandQueueOnScopeExit(computeCommandQueue);

            auto& computeCommandList = computeCommandQueue.GetCommandList();

            computeCommandList.SetPipelineState(*m_IrradiancePipelineState);
            computeCommandList.SetResourceBarrier(*irradianceTexture, benzin::ResourceState::UnorderedAccess);

            BenzinAssert(cubeTexture.HasShaderResourceView());
            computeCommandList.SetRootShaderResource(RootConstant::InputCubeTextureIndex, cubeTexture.GetShaderResourceView());
            computeCommandList.SetRootUnorderedAccess(RootConstant::OutIrradianceTextureIndex, irradianceTexture->GetUnorderedAccessView());

            computeCommandList.Dispatch({ textureSize, textureSize, textureArraySize }, { 8, 8, 1 });

            computeCommandList.SetResourceBarrier(*irradianceTexture, benzin::ResourceState::Present);
        }

        return irradianceTexture;
    }

} // namespace sandbox
