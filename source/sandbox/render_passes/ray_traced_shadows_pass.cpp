#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/ray_traced_shadows_pass.hpp"

#include <benzin/core/asserter.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/backend.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

namespace sandbox
{

    static constexpr auto g_RayGenShaderName = L"RayGeneneration"sv;
    static constexpr auto g_MissShaderName = L"Miss"sv;

    static constexpr auto g_HitGroupName = L"HitGroup"sv;
    static constexpr auto g_ClosestHitShaderName = L"ClosestHitShader"sv;

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

    RayTracedShadowsPass::RayTracedShadowsPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        m_Pso = ms_Device->GetPipelineStateManager().CreatePipelineState(benzin::RayTracingPipelineStateCreation
        {
            .DebugName = "RayTracedShadowsPass",
            .ShaderLibrary
            {
                .FileName = "ray_traced_shadows_pass.hlsl",
            },
            .HitGroup
            {
                .Name = "HitGroup", // TODO: g_HitGroupName to narrow string
                .ClosestHitEntryPoint = "ClosestHit",
            },
            .ShaderConfig
            {
                .PayloadSize = sizeof(joint::ShadowRayPayload),
                .AttributeSize = sizeof(DirectX::XMFLOAT2), // Barycentrics
            },
        });

        CreateShaderTable();

        benzin::MakeUniquePtr(m_PassConstBuffer, *ms_Device, "RayTracedShadowsConsts");
    }

    RayTracedShadowsPass::~RayTracedShadowsPass()
    {
        ms_Resources->DestroyTexture(+Texture::NoisyPenumbra);
        m_BlueNoise.reset();
    }

    void RayTracedShadowsPass::OnZeroFrameInit()
    {
        benzin::TextureImage blueNoiseImage;
        benzin::LoadTextureImageFromDdsFile("blue_noise_64.dds", blueNoiseImage);

        m_BlueNoise = std::make_unique<benzin::Texture>(*ms_Device, benzin::TextureCreation
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

    void RayTracedShadowsPass::OnRenderViewportResize()
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

    void RayTracedShadowsPass::OnUpdate()
    {
        const auto& shadowSettings = ms_Settings->GetSection<RayTracingShadowsSettings>();
        const auto& lightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        const float sunAngularRadiusInRadians = 0.5f * lightingSettings.SunAngularDiameterInRadians;
        const DirectX::XMFLOAT3 toSunDirection = GetSunDirection(lightingSettings);

        DirectX::XMFLOAT3 toSunTangent;
        DirectX::XMFLOAT3 toSunBitangent;
        BuildOrthonormalBasis(toSunDirection, toSunTangent, toSunBitangent);

        m_PassConstBuffer->UpdateConstants(joint::RayTracedShadowsConsts
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

    void RayTracedShadowsPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        auto* d3d12CommandList = commandList.GetD3D12GraphicsCommandList();
        BenzinPushGpuEvent(commandList, "RayTracingShadowPass");

        const auto& noisyPenumbra = ms_Resources->GetTexture(+Texture::NoisyPenumbra);

        commandList.SetPipelineState(*m_Pso);
        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_PassConstBuffer->GetActiveGpuVirtualAddress());

        {
            using enum joint::Rc_RayTracedShadows;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+Depth, ms_Resources->GetTexture(+Texture::DepthStencil).GetSrv());
            commandList.SetRootResource(+BlueNoise, m_BlueNoise->GetSrv());

            commandList.SetRootResource(+OutNoisyPenumbra, noisyPenumbra.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ noisyPenumbra, benzin::ResourceState::UnorderedAccess },
        );

        const D3D12_DISPATCH_RAYS_DESC d3d12DispatchRayDesc
        {
            .RayGenerationShaderRecord
            {
                .StartAddress = m_RayGenShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_RayGenShaderTable->GetNotAlignedSize(),
            },
            .MissShaderTable
            {
                .StartAddress = m_MissShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_MissShaderTable->GetNotAlignedSize(),
                .StrideInBytes = m_MissShaderTable->GetElementSize(),
            },
            .HitGroupTable
            {
                .StartAddress = m_HitGroupShaderTable->GetGpuVirtualAddress(),
                .SizeInBytes = m_HitGroupShaderTable->GetNotAlignedSize(),
                .StrideInBytes = m_HitGroupShaderTable->GetElementSize(),
            },
            .CallableShaderTable
            {
                .StartAddress = 0,
                .SizeInBytes = 0,
                .StrideInBytes = 0,
            },
            .Width = noisyPenumbra.GetWidth(),
            .Height = noisyPenumbra.GetHeight(),
            .Depth = 1,
        };

        d3d12CommandList->DispatchRays(&d3d12DispatchRayDesc);
    }

    void RayTracedShadowsPass::CreateShaderTable()
    {
        BenzinEnsure(m_Pso->GetD3D12StateObject() != nullptr);

        ComPtr<ID3D12StateObjectProperties> d3d12StateObjectProperties;
        BenzinEnsure(m_Pso->GetD3D12StateObject()->QueryInterface(IID_PPV_ARGS(&d3d12StateObjectProperties)));

        const auto CreateShaderTable = [&](std::wstring_view identiferName)
        {
            const void* rawShaderIdentifier = d3d12StateObjectProperties->GetShaderIdentifier(identiferName.data());
            const auto shaderIdentifier = std::span{ (const std::byte*)rawShaderIdentifier, benzin::GfxConfig::s_ShaderIdentifierSize };

            auto shaderTableBuffer = std::make_unique<benzin::Buffer>(*ms_Device, benzin::BufferCreation
            {
                .DebugName = std::format("{}ShaderTable", benzin::ToNarrowString(identiferName)),
                .MemoryType = benzin::ResourceMemoryType::Upload,
                .ElementSize = benzin::GfxConfig::s_RayTracingShaderRecordAlignment,
                .ElementCount = 1,
            });

            const benzin::MemoryWriter shaderTableWriter{ shaderTableBuffer->GetCpuMappedData(), shaderTableBuffer->GetSize() };
            shaderTableWriter.WriteBytes(shaderIdentifier);

            return shaderTableBuffer;
        };

        m_RayGenShaderTable = CreateShaderTable(g_RayGenShaderName);
        m_MissShaderTable = CreateShaderTable(g_MissShaderName);
        m_HitGroupShaderTable = CreateShaderTable(g_HitGroupName);
    }

}
