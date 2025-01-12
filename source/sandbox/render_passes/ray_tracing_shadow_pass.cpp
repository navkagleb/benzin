#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_tracing_shadow_pass.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/engine/resource_loader.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/ray_tracing_shader_table.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <shaders/joint/ray_tracing_shadow_resources.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

namespace sandbox
{

    static constexpr auto g_HitGroupName = "DefaultHitGroup"sv;

    static void BuildOrthonormalBasis(DirectX::XMFLOAT3 normal3, DirectX::XMFLOAT3& outTangent3, DirectX::XMFLOAT3& outBitangent3)
    {
        const DirectX::XMVECTOR up = abs(normal3.y) < 0.9999f ? DirectX::XMVECTOR{ 0.0f, 1.0f, 0.0f } : DirectX::XMVECTOR{ 1.0f, 0.0f, 0.0f };
        const DirectX::XMVECTOR normal = DirectX::XMLoadFloat3(&normal3);

        const DirectX::XMVECTOR tangent = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(up, normal));
        const DirectX::XMVECTOR bitangent = DirectX::XMVector3Cross(normal, tangent);

        DirectX::XMStoreFloat3(&outTangent3, tangent);
        DirectX::XMStoreFloat3(&outBitangent3, bitangent);
    }

    //

    RayTracing_ShadowPass::RayTracing_ShadowPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::RayTracingPipelineStateCreation
        {
            .DebugName = "RayTracing_ShadowPass",
            .ShaderLibrary
            {
                .FileName = "ray_tracing_shadow_pass.hlsl",
            },
            .HitGroup
            {
                .Name = g_HitGroupName,
                .ClosestHitEntryPoint = "ClosestHit",
            },
            .ShaderConfig
            {
                .PayloadSize = sizeof(joint::RayTracing_ShadowPayload),
                .AttributeSize = sizeof(DirectX::XMFLOAT2), // Barycentrics
            },
        });

        BuildShaderTable();

        benzin::MakeUniquePtr(m_PassConstBuffer, *ms_Device, "RayTracing_ShadowConsts");
    }

    RayTracing_ShadowPass::~RayTracing_ShadowPass()
    {
        ms_Device->GetPipelineStateManager().DestroyPipelineState(m_Pso);
        ms_Resources->DestroyTexture(+Texture::NoisyPenumbra);
    }

    void RayTracing_ShadowPass::OnZeroFrameInit()
    {
        benzin::TextureImage blueNoiseImage;
        benzin::LoadTextureImageFromDdsFile("blue_noise_64.dds", blueNoiseImage);

        benzin::MakeUniquePtr(m_BlueNoise, *ms_Device, benzin::TextureCreation
        {
            .DebugName = "BlueNoise64",
            .Format = blueNoiseImage.Format,
            .Width = blueNoiseImage.Width,
            .Height = blueNoiseImage.Height,
            .MipCount = 1,
        });

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList(m_BlueNoise->GetSize());
        commandList.UploadToTextureTopMip(*m_BlueNoise, std::as_bytes(std::span{ blueNoiseImage.ImageData }));
    }

    void RayTracing_ShadowPass::OnRenderViewportResize()
    {
        ms_Resources->CreateTexture(+Texture::NoisyPenumbra, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::NoisyPenumbra),
            .Format = SigmaDenoiserPass::s_PenumbraFormat,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    void RayTracing_ShadowPass::OnUpdate()
    {
        const auto& shadowSettings = ms_Settings->GetSection<RayTracingShadowsSettings>();
        const auto& lightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        const float sunAngularRadiusInRadians = 0.5f * lightingSettings.SunAngularDiameterInRadians;
        const DirectX::XMFLOAT3 toSunDirection = GetSunDirection(lightingSettings);

        DirectX::XMFLOAT3 toSunTangent;
        DirectX::XMFLOAT3 toSunBitangent;
        BuildOrthonormalBasis(toSunDirection, toSunTangent, toSunBitangent);

        m_PassConstBuffer->UpdateConstants(joint::RayTracing_ShadowConsts
        {
            .ToSunDirection = toSunDirection,
            .TanSunAngularRadius = std::tan(sunAngularRadiusInRadians),
            .ToSunTangent = toSunTangent,
            .SunAngularRadiusInRadians = sunAngularRadiusInRadians,
            .ToSunBitangent = toSunBitangent,
            .IsBlueNoiseUsed = shadowSettings.IsBlueNoiseUsed,
            .IsNoiseAnimated = shadowSettings.IsNoiseAnimated,
        });
    }

    void RayTracing_ShadowPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "RayTracingShadowPass");

        const auto& noisyPenumbra = ms_Resources->GetTexture(+Texture::NoisyPenumbra);

        commandList.SetPipelineState(*m_Pso);
        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstBuffer->GetActiveGpuVirtualAddress());

        {
            using enum joint::Rc_RayTracing_Shadow;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+Depth, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
            commandList.SetRootResource(+BlueNoise, m_BlueNoise->GetSrv());

            commandList.SetRootResource(+OutNoisyPenumbra, noisyPenumbra.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ noisyPenumbra, benzin::ResourceState::UnorderedAccess },
        );

        commandList.DispatchRays(*m_ShaderTable, { GetRenderViewportWidth(), GetRenderViewportHeight(), 1 });
    }

    void RayTracing_ShadowPass::BuildShaderTable()
    {
        BenzinEnsure(m_Pso->GetD3D12StateObject() != nullptr);

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinEnsure(m_Pso->GetD3D12StateObject()->QueryInterface(IID_PPV_ARGS(&d3d12StateObjectProperties)));

        const auto getShaderIdentifier = [&d3d12StateObjectProperties](std::string_view idName)
        {
            const std::wstring wideIdName = benzin::ToWideString(idName);
            const void* rawId = d3d12StateObjectProperties->GetShaderIdentifier(wideIdName.data());

            return rawId;
        };

        benzin::MakeUniquePtr(m_ShaderTable);
        m_ShaderTable->SetRayGenerationShader(getShaderIdentifier("RayGeneration"));
        m_ShaderTable->SetMissShader(getShaderIdentifier("Miss"));
        m_ShaderTable->SetHitGroupShaders(getShaderIdentifier(g_HitGroupName));

        m_TableBuffer = std::make_unique<benzin::Buffer>(*ms_Device, benzin::BufferCreation
        {
            .DebugName = "RayTracedShadows_ShaderTable",
            .MemoryType = benzin::ResourceMemoryType::Upload,
            .ElementSize = (uint32_t)m_ShaderTable->GetRequiredTableSize(),
            .ElementCount = 1,
        });

        m_ShaderTable->UploadToGpu(m_TableBuffer.get());
    }

}
