#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/pipeline_state_manager.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

#include <shaders/joint/sigma_denoiser_resources.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"

BenzinEnableUnaryPlusForEnum(sandbox::SigmaDenoiserPass::Step);

namespace sandbox
{


    static uint32_t GetMaxHistoryLength(float fps)
    {
        // TODO: Provide smooth fps

        constexpr float defaultAccumulationTimeInSec = 0.084f; // 5 (history length) / 60 (fps)

        const auto allowedMaxHistoryLength = (uint32_t)(defaultAccumulationTimeInSec * fps);
        return std::min(allowedMaxHistoryLength, SigmaDenoiserPass::s_MaxHistoryLength);
    }

    //

    const benzin::GraphicsFormat SigmaDenoiserPass::s_PenumbraFormat = benzin::GraphicsFormat::R16Float;
    const uint32_t SigmaDenoiserPass::s_MaxHistoryLength = 7;

    SigmaDenoiserPass::SigmaDenoiserPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        auto& psoManager = ms_Device->GetPipelineStateManager();
        m_Psos[+Step::ClassifyTiles] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_ClassifyTiles", .CsFileName = "sigma_denoiser/classify_tiles.hlsl" });
        m_Psos[+Step::SmoothTiles] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_SmoothTiles", .CsFileName = "sigma_denoiser/smooth_tiles.hlsl" });
        m_Psos[+Step::CopyHistory] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_CopyHistory", .CsFileName = "sigma_denoiser/copy_history.hlsl" });
        m_Psos[+Step::Blur] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_Blur", .CsFileName = "sigma_denoiser/blur.hlsl", .CsDefines{ "FIRST_BLUR_PASS"} });
        m_Psos[+Step::PostBlur] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_PostBlur", .CsFileName = "sigma_denoiser/blur.hlsl" });
        m_Psos[+Step::TemporalStabilization] = psoManager.CreatePipelineState(benzin::ComputePipelineStateCreation{ .DebugName = "Sigma_TemporalStabilization", .CsFileName = "sigma_denoiser/temporal_stabilization.hlsl" });

        MakeUniquePtr(m_SigmaConstantBuffer, *ms_Device, "SigmaConstantBuffer");
    }

    SigmaDenoiserPass::~SigmaDenoiserPass()
    {
        auto& psoManager = ms_Device->GetPipelineStateManager();
        for (auto*& pso : m_Psos)
        {
            psoManager.DestroyPipelineState(pso);
        }

        ms_Resources->DestroyTexture(+Texture::Sigma_Tiles);
        ms_Resources->DestroyTexture(+Texture::Sigma_SmoothTiles);
        ms_Resources->DestroyTexture(+Texture::Sigma_BlurredPenumbra1);
        ms_Resources->DestroyTexture(+Texture::Sigma_BlurredPenumbra2);
        ms_Resources->DestroyTexture(+Texture::Sigma_BlurredShadowTemp1);
        ms_Resources->DestroyTexture(+Texture::Sigma_BlurredShadowTemp2);
        ms_Resources->DestroyTexture(+Texture::Shadow);
        ms_Resources->DestroyTexture(+Texture::ShadowHistoryLength);
    }

    void SigmaDenoiserPass::OnRenderViewportResize()
    {
        m_TileCount.x = benzin::DivideUp(GetRenderViewportWidth(), joint::g_SigmaTileSize);
        m_TileCount.y = benzin::DivideUp(GetRenderViewportHeight(), joint::g_SigmaTileSize);

        const DirectX::XMUINT2 renderResolution = GetRenderResolution();
        const auto shadowFormat = benzin::GraphicsFormat::R8Unorm;

        const auto createTexture = [](Texture texture, benzin::GraphicsFormat format, DirectX::XMUINT2 resolution)
        {
            ms_Resources->CreateTexture(+texture, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(texture),
                .Format = format,
                .Width = resolution.x,
                .Height = resolution.y,
                .MipCount = 1,
                .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
            });
        };

        createTexture(Texture::Sigma_Tiles, benzin::GraphicsFormat::Rgba8Unorm, m_TileCount);
        createTexture(Texture::Sigma_SmoothTiles, benzin::GraphicsFormat::Rg8Unorm, m_TileCount);
        createTexture(Texture::Sigma_BlurredPenumbra1, s_PenumbraFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredPenumbra2, s_PenumbraFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredShadowTemp1, shadowFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredShadowTemp2, shadowFormat, renderResolution);
        createTexture(Texture::Shadow, shadowFormat, renderResolution);
        createTexture(Texture::ShadowHistoryLength, benzin::GraphicsFormat::R32Uint, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate(const benzin::TickTimer& tickTimer)
    {
        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = benzin::GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = benzin::GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = benzin::GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        auto& sigmaSettings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        const auto& lightingSettings = ms_Settings->GetSection<DeferredLightingSettings>();

        const float fps = 1.0f / tickTimer.GetDeltaTimeInSec();
        sigmaSettings.MaxHistoryLength = GetMaxHistoryLength(fps);
        sigmaSettings.StabilizationStrength = sigmaSettings.MaxHistoryLength / (1.0f + sigmaSettings.MaxHistoryLength);

        const auto& shadowSettings = ms_Settings->GetSection<RayTracing_ShadowSettings>();
        const auto worldLightPosition = m_Scene.GetEntityRegistry().get<benzin::TransformComponent>(shadowSettings.LightHandle).GetTranslation();

        m_SigmaConstantBuffer->UpdateConstants(joint::SigmaConstants
        {
            .StabilizationStrength = sigmaSettings.StabilizationStrength,
            .WorldSunDirection = GetSunDirection(lightingSettings),
            .IsShadowsFromSun = shadowSettings.IsShadowsFromSun,
            .WorldLightPosition = worldLightPosition,
            .BlurRotator = blurRotator,
            .PostBlurRotator = postBlurRotator,
            .TileCount = m_TileCount,
            .PlaneDistanceSensitivity = sigmaSettings.PlaneDistanceSensitivity,
            .DisocclusionThreshold = sigmaSettings.DisocclusionThreshold,
            .IsTileSmoothingEnabled = sigmaSettings.IsTileSmoothingEnabled,
        });
    }

    void SigmaDenoiserPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "SigmaDenoiserPass");

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer, m_SigmaConstantBuffer->GetActiveGpuVirtualAddress());

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (settings.IsEnabled)
        {
            RunClearPass(settings.IsClearEnabled);
            RunClassifyTilesPass();
            RunSmoothTilesPass();
            RunBlurPass();
            RunPostBlurPass(settings.IsPostBlurEnabled);
            RunTemporalStabilizationPass(settings.IsTemporalStabilizationEnabled);
        }
    }

    void SigmaDenoiserPass::RunClearPass(bool isEnabled) const
    {
        if (!isEnabled)
        {
            return;
        }

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "Clear");

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_Tiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2), benzin::ResourceState::UnorderedAccess },
        );

        const DirectX::XMFLOAT4 clearColor{};
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_Tiles), clearColor);
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles), clearColor);
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1), clearColor);
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2), clearColor);
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1), clearColor);
        commandList.ClearUnorderedAccess(ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2), clearColor);
    }

    void SigmaDenoiserPass::RunClassifyTilesPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "ClassifyTiles");

        const auto& tiles = ms_Resources->GetTexture(+Texture::Sigma_Tiles);

        {
            using enum joint::Rc_SigmaClassifyTiles;

            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());

            commandList.SetRootResource(+OutTiles, tiles.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ tiles, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPipelineState(*m_Psos[+Step::ClassifyTiles]);
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "SmoothTiles");

        const auto& smoothTiles = ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles);

        {
            using enum joint::Rc_SigmaSmoothTiles;

            commandList.SetRootResource(+Tiles, ms_Resources->GetTexture(+Texture::Sigma_Tiles).GetSrv());
            commandList.SetRootResource(+OutSmoothTiles, smoothTiles.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ smoothTiles, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPipelineState(*m_Psos[+Step::SmoothTiles]);
        commandList.Dispatch({ m_TileCount.x, m_TileCount.y, 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunBlurPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "Blur");

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1);

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+SmoothTiles, ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());

            commandList.SetRootResource(+OutPenumbra, penumbra1.GetUav());
            commandList.SetRootResource(+OutShadow, shadowTemp1.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPipelineState(*m_Psos[+Step::Blur]);
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunPostBlurPass(bool isEnabled) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "PostBlur");

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1);
        const auto& penumbra2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1);
        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2);

        if (!isEnabled)
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

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+SmoothTiles, ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles).GetSrv());
            commandList.SetRootResource(+Penumbra, penumbra1.GetSrv());
            commandList.SetRootResource(+Shadow, shadowTemp1.GetSrv());

            commandList.SetRootResource(+OutPenumbra, penumbra2.GetUav());
            commandList.SetRootResource(+OutShadow, shadowTemp2.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPipelineState(*m_Psos[+Step::PostBlur]);
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass(bool isEnabled) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "TemporalStabilization");

        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2);
        const auto& shadow = ms_Resources->GetTexture(+Texture::Shadow);

        if (!isEnabled)
        {
            BenzinMakeScopedResourceBarriers(
                commandList,
                benzin::TransitionBarrier{ shadow, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::CopySource },
            );

            commandList.CopyResource(shadow, shadowTemp2);

            return;
        }

        const auto& historyLength = ms_Resources->GetTexture(+Texture::ShadowHistoryLength);

        {
            using enum joint::Rc_SigmaTemporalStabilization;

            commandList.SetRootResource(+Mv, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+SmoothTiles, ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2).GetSrv());
            commandList.SetRootResource(+Shadow, shadowTemp2.GetSrv());
            commandList.SetRootResource(+ShadowHistory, ms_Resources->GetPrevTexture(+Texture::Shadow).GetSrv());
            commandList.SetRootResource(+HistoryLength, ms_Resources->GetPrevTexture(+Texture::ShadowHistoryLength).GetSrv());

            commandList.SetRootResource(+OutShadow, shadow.GetUav());
            commandList.SetRootResource(+OutHistoryLength, historyLength.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ shadow, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ historyLength, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPipelineState(*m_Psos[+Step::TemporalStabilization]);
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

}
