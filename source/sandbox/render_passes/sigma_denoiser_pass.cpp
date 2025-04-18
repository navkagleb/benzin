#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"

BenzinEnableUnaryPlusForEnum(joint::SigmaClassifyTilesResources);
BenzinEnableUnaryPlusForEnum(joint::SigmaSmoothTilesResources);
BenzinEnableUnaryPlusForEnum(joint::SigmaBlurResources);
BenzinEnableUnaryPlusForEnum(joint::SigmaTemporalStabilizationResources)

namespace sandbox
{

    static uint32_t GetMaxHistoryLength(uint32_t maxHistoryLength, float fps)
    {
        // TODO: Provide smooth fps

        constexpr float defaultAccumulationTimeInSec = 0.084f; // 5 (history length) / 60 (fps)

        const auto allowedMaxHistoryLength = (uint32_t)(defaultAccumulationTimeInSec * fps);
        return std::min(allowedMaxHistoryLength, maxHistoryLength);
    }

    //

    SigmaDenoiserPass::SigmaDenoiserPass()
    {
        const auto createPso = [](PsoId id, std::string_view fileName, std::string_view define = {})
        {
            ms_PsoManager->Create(id, [id, fileName, define](benzin::ComputePsoProxy& proxy)
            {
                proxy.Cs.FileName = fileName;

                if (!define.empty())
                {
                    proxy.Cs.Defines.push_back(define);
                }
            });
        };

        createPso(PsoId::SigmaClassifyTiles, "sigma_denoiser/classify_tiles.hlsl");
        createPso(PsoId::SigmaSmoothTiles, "sigma_denoiser/smooth_tiles.hlsl");
        createPso(PsoId::SigmaBlur, "sigma_denoiser/blur.hlsl");
        createPso(PsoId::SigmaPostBlur, "sigma_denoiser/blur.hlsl", "POST_BLUR_PASS");
        createPso(PsoId::SigmaTemporalStabilization, "sigma_denoiser/temporal_stabilization.hlsl");

        m_PerLightConsts.resize(benzin::Scene::s_MaxLightCount);

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
        ms_ConstBufferPool->PreAllocate(sizeof(joint::SigmaPerLightConsts), benzin::Scene::s_MaxLightCount);
    }

    SigmaDenoiserPass::~SigmaDenoiserPass()
    {
        ms_PsoManager->Destroy(PsoId::SigmaClassifyTiles);
        ms_PsoManager->Destroy(PsoId::SigmaSmoothTiles);
        ms_PsoManager->Destroy(PsoId::SigmaBlur);
        ms_PsoManager->Destroy(PsoId::SigmaPostBlur);
        ms_PsoManager->Destroy(PsoId::SigmaTemporalStabilization);

        ms_Resources->Destroy(TextureId::Sigma_Tiles);
        ms_Resources->Destroy(TextureId::Sigma_SmoothTiles);
        ms_Resources->Destroy(TextureId::Sigma_BlurredPenumbra1);
        ms_Resources->Destroy(TextureId::Sigma_BlurredPenumbra2);
        ms_Resources->Destroy(TextureId::Sigma_BlurredShadowTemp1);
        ms_Resources->Destroy(TextureId::Sigma_BlurredShadowTemp2);
        ms_Resources->Destroy(TextureId::Shadow);
        ms_Resources->Destroy(TextureId::ShadowHistoryLength);
    }

    void SigmaDenoiserPass::OnRenderViewportResize()
    {
        m_Consts.TileCount.x = benzin::DivideUp(GetRenderViewportWidth(), joint::g_SigmaTileSize);
        m_Consts.TileCount.y = benzin::DivideUp(GetRenderViewportHeight(), joint::g_SigmaTileSize);

        const DirectX::XMUINT2 renderResolution = GetRenderResolution();
        const auto shadowFormat = benzin::GraphicsFormat::R8Unorm;

        const auto createTexture = [](TextureId id, benzin::GraphicsFormat format, DirectX::XMUINT2 resolution, uint16_t depth = 1)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(id),
                .Format = format,
                .Width = resolution.x,
                .Height = resolution.y,
                .Depth = depth,
                .MipCount = 1,
                .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
            });
        };

        const auto penumbraFormat = ms_Settings->GetSection<SigmaDenoiserSettings>().PenumbraFormat;

        createTexture(TextureId::Sigma_Tiles, benzin::GraphicsFormat::Rgba8Unorm, m_Consts.TileCount);
        createTexture(TextureId::Sigma_SmoothTiles, benzin::GraphicsFormat::Rg8Unorm, m_Consts.TileCount);
        createTexture(TextureId::Sigma_BlurredPenumbra1, penumbraFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredPenumbra2, penumbraFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredShadowTemp1, shadowFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredShadowTemp2, shadowFormat, renderResolution);
        createTexture(TextureId::Shadow, shadowFormat, renderResolution, benzin::Scene::s_MaxLightCount);
        createTexture(TextureId::ShadowHistoryLength, benzin::GraphicsFormat::R32Uint, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate()
    {
        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = benzin::GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = benzin::GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = benzin::GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const float fps = 1.0f / ms_FrameTimer->GetDeltaTimeInSec();
        settings.HistoryLength = GetMaxHistoryLength(settings.MaxHistoryLength, fps);
        settings.StabilizationStrength = settings.HistoryLength / (1.0f + settings.HistoryLength);

        m_Consts.BlurRotator = blurRotator;
        m_Consts.PostBlurRotator = postBlurRotator;
        m_Consts.StabilizationStrength = settings.StabilizationStrength;
        m_Consts.PlaneDistanceSensitivity = settings.PlaneDistanceSensitivity;
        m_Consts.DisocclusionThreshold = settings.DisocclusionThreshold;
        m_Consts.IsTileSmoothingEnabled = settings.IsTileSmoothingEnabled;

        {
            const auto& entityRegistry = ms_Scene->GetEntityRegistry();
            uint32_t lightOffset = 0;

            {
                const auto& light = entityRegistry.get<benzin::SunLight>(ms_Scene->GetSunEntity());

                m_PerLightConsts[0].LightType = joint::LightType::Sun;
                m_PerLightConsts[0].WorldLightPosition = light.CalcToSunDirection();
                lightOffset++;
            }


            const auto view = entityRegistry.view<benzin::SphericalLight>();
            for (const auto& [_, light] : view.each())
            {
                if (!light.IsEnabled())
                {
                    continue;
                }

                m_PerLightConsts[lightOffset].LightType = joint::LightType::Spherical;
                m_PerLightConsts[lightOffset].WorldLightPosition = light.GetPosition();
                lightOffset++;
            }
        }
    }

    void SigmaDenoiserPass::OnRender() const
    {
        BenzinProfile();

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsEnabled)
        {
            return;
        }

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "SigmaDenoiser");

        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        const uint32_t lightCount = ms_Scene->GetActiveLightCount();
        for (uint16_t sliceIndex = 0; sliceIndex < lightCount; ++sliceIndex)
        {
            BenzinScopeProfile(std::format("Step: {}", sliceIndex));

            if (lightCount != 1)
            {
                BenzinGpuEvent(cmdList, "Step");
            }

            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer1, ms_ConstBufferPool->Allocate(m_PerLightConsts[sliceIndex]));

            RunClearPass(settings.IsClearEnabled);
            RunClassifyTilesPass(sliceIndex);
            RunSmoothTilesPass();
            RunBlurPass(sliceIndex);
            RunPostBlurPass(settings.IsPostBlurEnabled);
            RunTemporalStabilizationPass(settings.IsTemporalStabilizationEnabled, sliceIndex);
        }
    }

    void SigmaDenoiserPass::RunClearPass(bool isEnabled) const
    {
        BenzinProfile();

        if (!isEnabled)
        {
            return;
        }

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "Clear");

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_Tiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_SmoothTiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2), benzin::ResourceState::UnorderedAccess }
        );

        const DirectX::XMFLOAT4 clearColor{};
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_Tiles), ms_Resources->Get(TextureId::Sigma_Tiles).GetUav(), clearColor);
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_SmoothTiles), ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetUav(), clearColor);
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1), ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1).GetUav(), clearColor);
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2), ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2).GetUav(), clearColor);
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1), ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1).GetUav(), clearColor);
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2), ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2).GetUav(), clearColor);
    }

    void SigmaDenoiserPass::RunClassifyTilesPass(uint16_t sliceIndex) const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ClassifyTiles");

        const auto& tiles = ms_Resources->Get(TextureId::Sigma_Tiles);

        {
            using enum joint::SigmaClassifyTilesResources;

            cmdList.SetComputeRootResource(+ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra).GetSrv({ .DepthRange = sliceIndex }));

            cmdList.SetComputeRootResource(+OutTiles, tiles.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ tiles, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaClassifyTiles));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "SmoothTiles");

        const auto& smoothTiles = ms_Resources->Get(TextureId::Sigma_SmoothTiles);

        {
            using enum joint::SigmaSmoothTilesResources;

            cmdList.SetComputeRootResource(+Tiles, ms_Resources->Get(TextureId::Sigma_Tiles).GetSrv());
            cmdList.SetComputeRootResource(+OutSmoothTiles, smoothTiles.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ smoothTiles, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaSmoothTiles));
        cmdList.Dispatch({ m_Consts.TileCount.x, m_Consts.TileCount.y, 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunBlurPass(uint16_t sliceIndex) const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "Blur");

        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);

        {
            using enum joint::SigmaBlurResources;

            cmdList.SetComputeRootResource(+WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(+ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra).GetSrv({ .DepthRange = sliceIndex }));

            cmdList.SetComputeRootResource(+OutPenumbra, penumbra1.GetUav());
            cmdList.SetComputeRootResource(+OutShadow, shadowTemp1.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaBlur));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunPostBlurPass(bool isEnabled) const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "PostBlur");

        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& penumbra2 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2);
        const auto& shadowTemp1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);
        const auto& shadowTemp2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);

        if (!isEnabled)
        {
            BenzinScopedResourceBarriers(
                cmdList,
                benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::CopySource },
                benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::CopySource }
            );

            cmdList.CopyResource(penumbra2, penumbra1);
            cmdList.CopyResource(shadowTemp2, shadowTemp1);
        
            return;
        }

        {
            using enum joint::SigmaBlurResources;

            cmdList.SetComputeRootResource(+WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(+ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Penumbra, penumbra1.GetSrv());
            cmdList.SetComputeRootResource(+Shadow, shadowTemp1.GetSrv());

            cmdList.SetComputeRootResource(+OutPenumbra, penumbra2.GetUav());
            cmdList.SetComputeRootResource(+OutShadow, shadowTemp2.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaPostBlur));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass(bool isEnabled, uint16_t sliceIndex) const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "TemporalStabilization");

        const auto& shadowTemp2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);
        const auto& shadow = ms_Resources->Get(TextureId::Shadow);

        if (!isEnabled)
        {
            BenzinScopedResourceBarriers(
                cmdList,
                benzin::TransitionBarrier{ shadow, benzin::ResourceState::CopyDestination },
                benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::CopySource }
            );

            cmdList.CopyResource(shadow, shadowTemp2);

            return;
        }

        const auto& historyLength = ms_Resources->Get(TextureId::ShadowHistoryLength);

        {
            using enum joint::SigmaTemporalStabilizationResources;

            cmdList.SetComputeRootResource(+Mv, ms_Resources->Get(TextureId::Mv).GetSrv());
            cmdList.SetComputeRootResource(+ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Penumbra, ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2).GetSrv());
            cmdList.SetComputeRootResource(+Shadow, shadowTemp2.GetSrv());
            cmdList.SetComputeRootResource(+ShadowHistory, ms_Resources->GetPrev(TextureId::Shadow).GetSrv({ .DepthRange = sliceIndex }));
            cmdList.SetComputeRootResource(+HistoryLength, ms_Resources->GetPrev(TextureId::ShadowHistoryLength).GetSrv());

            cmdList.SetComputeRootResource(+OutShadow, shadow.GetUav({ .DepthRange = sliceIndex }));
            cmdList.SetComputeRootResource(+OutHistoryLength, historyLength.GetUav());
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ shadow, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ historyLength, benzin::ResourceState::UnorderedAccess }
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaTemporalStabilization));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

}
