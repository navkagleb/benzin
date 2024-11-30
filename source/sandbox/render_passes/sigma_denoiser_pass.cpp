#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/utility/benzin_defines.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"

namespace sandbox
{

    SigmaDenoiserPass::SigmaDenoiserPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        auto& psoManager = ms_Device->GetPipelineStateManager();
        m_ClassifyTilesPso = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_ClassifyTiles", .CsFileName = "sigma_denoiser/classify_tiles.hlsl" });
        m_SmoothTilesPso = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_SmoothTiles", .CsFileName = "sigma_denoiser/smooth_tiles.hlsl" });
        m_BlurPso = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_Blur", .CsFileName = "sigma_denoiser/blur.hlsl", .CsDefines{ "FIRST_BLUR_PASS"} });
        m_PostBlurPso = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_PostBlur", .CsFileName = "sigma_denoiser/blur.hlsl" });
        m_TemporalStabilizationPso = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_TemporalStabilization", .CsFileName = "sigma_denoiser/temporal_stabilization.hlsl" });

        MakeUniquePtr(m_SigmaConstantBuffer, *ms_Device, "SigmaConstantBuffer");
    }

    SigmaDenoiserPass::~SigmaDenoiserPass()
    {
        auto& psoManager = ms_Device->GetPipelineStateManager();
        psoManager.DestroyPipelineState(m_ClassifyTilesPso);
        psoManager.DestroyPipelineState(m_SmoothTilesPso);
        psoManager.DestroyPipelineState(m_BlurPso);
        psoManager.DestroyPipelineState(m_PostBlurPso);
        psoManager.DestroyPipelineState(m_TemporalStabilizationPso);

        ms_Resources->DestroyTexture(+Texture::SigmaTiles);
        ms_Resources->DestroyTexture(+Texture::SigmaSmoothTiles);
        ms_Resources->DestroyTexture(+Texture::SigmaPenumbra1);
        ms_Resources->DestroyTexture(+Texture::SigmaPenumbra2);
        ms_Resources->DestroyTexture(+Texture::SigmaShadowHistory);
        ms_Resources->DestroyTexture(+Texture::SigmaShadowTemp1);
        ms_Resources->DestroyTexture(+Texture::SigmaShadowTemp2);
        ms_Resources->DestroyTexture(+Texture::SigmaShadow);
    }

    void SigmaDenoiserPass::OnRenderViewportResize()
    {
        m_TileCount.x = benzin::DivideUp(GetRenderViewportWidth(), joint::g_SigmaTileSize);
        m_TileCount.y = benzin::DivideUp(GetRenderViewportHeight(), joint::g_SigmaTileSize);

        const auto createSigmaTexture = [](Texture textureIndex, benzin::GraphicsFormat format, DirectX::XMUINT2 resolution)
        {
            ms_Resources->CreateTexture(+textureIndex, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(textureIndex),
                .Format = format,
                .Width = resolution.x,
                .Height = resolution.y,
                .MipCount = 1,
                .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
            });
        };

        createSigmaTexture(Texture::SigmaTiles, benzin::GraphicsFormat::Rgba8Unorm, m_TileCount);
        createSigmaTexture(Texture::SigmaSmoothTiles, benzin::GraphicsFormat::Rg8Unorm, m_TileCount);

        const auto shadowFormat = benzin::GraphicsFormat::R32Float;
        const auto penumbraFormat = benzin::GraphicsFormat::R8Unorm;
        const DirectX::XMUINT2 renderResolution{ GetRenderViewportWidth(), GetRenderViewportHeight() };

        createSigmaTexture(Texture::SigmaPenumbra1, penumbraFormat, renderResolution);
        createSigmaTexture(Texture::SigmaPenumbra2, penumbraFormat, renderResolution);

        createSigmaTexture(Texture::SigmaShadowHistory, shadowFormat, renderResolution);
        createSigmaTexture(Texture::SigmaShadowTemp1, shadowFormat, renderResolution);
        createSigmaTexture(Texture::SigmaShadowTemp2, shadowFormat, renderResolution);
        createSigmaTexture(Texture::SigmaShadow, shadowFormat, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate()
    {
        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = benzin::GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = benzin::GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = benzin::GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        const auto& deferredLightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        m_SigmaConstantBuffer->UpdateConstants(joint::SigmaConstants
        {
            .TileCount = m_TileCount,
            .StabilizationStrength = sigmaSettings.StabilizationStrength,
            .WorldSunDirection = GetSunDirection(deferredLightingSettings),
            .BlurRotator = blurRotator,
            .PostBlurRotator = postBlurRotator,
        });
    }

    void SigmaDenoiserPass::OnRender() const
    {
        const auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "SigmaDenoiserPass");

        const auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (sigmaSettings.IsEnabled)
        {
            RunClassifyTilesPass();
            RunSmoothTilesPass();
            RunBlurPass();
            RunPostBlurPass();
            RunTemporalStabilizationPass();
        }
    }

    void SigmaDenoiserPass::RunClassifyTilesPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "ClassifyTiles");

        commandList.SetPipelineState(*m_ClassifyTilesPso);

        const auto& tilesTexture = ms_Resources->GetTexture(+Texture::SigmaTiles);

        {
            using enum joint::Rc_SigmaClassifyTiles;

            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());
            commandList.SetRootResource(+OutTilesTex, tilesTexture.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ tilesTexture, benzin::ResourceState::UnorderedAccess },
        );

        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "SmoothTiles");

        commandList.SetPipelineState(*m_SmoothTilesPso);
        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_SigmaConstantBuffer->GetActiveGpuVirtualAddress());

        const auto& smoothTilesTexture = ms_Resources->GetTexture(+Texture::SigmaSmoothTiles);

        {
            using enum joint::Rc_SigmaSmoothTiles;
            commandList.SetRootResource(+TilesTex, ms_Resources->GetTexture(+Texture::SigmaTiles).GetSrv());
            commandList.SetRootResource(+OutSmoothTilesTex, smoothTilesTexture.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ smoothTilesTexture, benzin::ResourceState::UnorderedAccess },
        );

        commandList.Dispatch({ smoothTilesTexture.GetWidth(), smoothTilesTexture.GetHeight(), 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunBlurPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "Blur");

        commandList.SetPipelineState(*m_BlurPso);

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::SigmaPenumbra1);
        const auto& shadowhistory = ms_Resources->GetTexture(+Texture::SigmaShadowHistory);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp1);

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormalTex, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());
            commandList.SetRootResource(+SmoothTilesTex, ms_Resources->GetTexture(+Texture::SigmaSmoothTiles).GetSrv());
            commandList.SetRootResource(+HistoryTex, ms_Resources->GetTexture(+Texture::SigmaShadow).GetSrv());
            commandList.SetRootResource(+OutHistoryTex, shadowhistory.GetUav());
            commandList.SetRootResource(+OutPenumbraTex, penumbra1.GetUav());
            commandList.SetRootResource(+OutShadowTex, shadowTemp1.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ shadowhistory, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::UnorderedAccess },
        );

        // commandList.ClearUnorderedAccess(penumbra1, { 0.0f, 0.0f, 0.0f, 0.0f });
        // commandList.ClearUnorderedAccess(shadow, { 0.0f, 0.0f, 0.0f, 0.0f });
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunPostBlurPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "PostBlur");

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::SigmaPenumbra1);
        const auto& penumbra2 = ms_Resources->GetTexture(+Texture::SigmaPenumbra2);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp1);
        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp2);

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsPostBlurEnabled)
        {
            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::CopySource },
                benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::CopySource },
            );

            commandList.CopyResource(penumbra2, penumbra1);
            commandList.CopyResource(shadowTemp2, shadowTemp1);

            return;
        }

        commandList.SetPipelineState(*m_PostBlurPso);

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormalTex, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::SigmaPenumbra1).GetSrv());
            commandList.SetRootResource(+SmoothTilesTex, ms_Resources->GetTexture(+Texture::SigmaSmoothTiles).GetSrv());
            commandList.SetRootResource(+ShadowTex, ms_Resources->GetTexture(+Texture::SigmaShadowTemp1).GetSrv());
            commandList.SetRootResource(+OutPenumbraTex, penumbra2.GetUav());
            commandList.SetRootResource(+OutShadowTex, shadowTemp2.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::UnorderedAccess },
        );

        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "TemporalStabilization");

        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp2);
        const auto& shadow = ms_Resources->GetTexture(+Texture::SigmaShadow);

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsTemporalStabilizationEnabled)
        {
            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ shadow, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::CopySource },
            );

            commandList.CopyResource(shadow, shadowTemp2);

            return;
        }

        commandList.SetPipelineState(*m_TemporalStabilizationPso);

        {
            using enum joint::Rc_SigmaTemporalStabilization;

            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+MvTex, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::SigmaPenumbra2).GetSrv());
            commandList.SetRootResource(+ShadowTex, shadowTemp2.GetSrv());
            commandList.SetRootResource(+HistoryTex, ms_Resources->GetTexture(+Texture::SigmaShadowHistory).GetSrv());
            commandList.SetRootResource(+SmoothTilesTex, ms_Resources->GetTexture(+Texture::SigmaSmoothTiles).GetSrv());
            commandList.SetRootResource(+OutShadowTex, shadow.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ shadow, benzin::ResourceState::UnorderedAccess },
        );

        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

}
