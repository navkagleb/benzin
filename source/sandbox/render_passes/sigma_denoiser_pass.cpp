#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/sigma_denoiser_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

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
        m_Consts.TileCount.x = benzin::DivideUp(ms_RenderViewportWidth, joint::g_SigmaTileSize);
        m_Consts.TileCount.y = benzin::DivideUp(ms_RenderViewportHeight, joint::g_SigmaTileSize);

        const DirectX::XMUINT2 renderResolution{ ms_RenderViewportWidth, ms_RenderViewportHeight };
        const auto shadowFormat = benzin::GraphicsFormat::R8Unorm;

        const auto createTexture = [](TextureId id, benzin::GraphicsFormat format, DirectX::XMUINT2 resolution)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(id),
                .Format = format,
                .Width = resolution.x,
                .Height = resolution.y,
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
        createTexture(TextureId::Shadow, shadowFormat, renderResolution);
        createTexture(TextureId::ShadowHistoryLength, benzin::GraphicsFormat::R32Uint, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate()
    {
        auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        m_IsRenderingEnabled = settings.IsEnabled;
        if (!m_IsRenderingEnabled)
            return;

        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = benzin::GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = benzin::GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = benzin::GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        const float fps = 1.0f / ms_FrameTimer->GetDeltaTimeInSec();
        settings.HistoryLength = GetMaxHistoryLength(settings.MaxHistoryLength, fps);
        settings.StabilizationStrength = settings.HistoryLength / (1.0f + settings.HistoryLength);

        m_Consts.BlurRotator = blurRotator;
        m_Consts.PostBlurRotator = postBlurRotator;
        m_Consts.StabilizationStrength = settings.StabilizationStrength;
        m_Consts.PlaneDistanceSensitivity = settings.PlaneDistanceSensitivity;
        m_Consts.DisocclusionThreshold = settings.DisocclusionThreshold;
        m_Consts.IsTileSmoothingEnabled = settings.IsTileSmoothingEnabled;
        m_Consts.ToSunDirection = ms_Scene->m_SunLight.CalcToSunDirection();
    }

    void SigmaDenoiserPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("SigmaDenoiser");

        auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        RunClearPass(settings.IsClearEnabled);
        RunClassifyTilesPass();
        RunSmoothTilesPass();
        RunBlurPass();
        RunPostBlurPass(settings.IsPostBlurEnabled);
        RunTemporalStabilizationPass(settings.IsTemporalStabilizationEnabled);
    }

    void SigmaDenoiserPass::RunClearPass(bool isEnabled) const
    {
        BenzinProfile();
        BenzinGpuProfile("Clear");

        if (!isEnabled)
            return;

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_Tiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_SmoothTiles), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1), benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2), benzin::ResourceState::UnorderedAccess });

        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_Tiles), ms_Resources->Get(TextureId::Sigma_Tiles).GetUav());
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_SmoothTiles), ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetUav());
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1), ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1).GetUav());
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2), ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2).GetUav());
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1), ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1).GetUav());
        cmdList.ClearUnorderedAccess(ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2), ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2).GetUav());
    }

    void SigmaDenoiserPass::RunClassifyTilesPass() const
    {
        BenzinProfile();
        BenzinGpuProfile("ClassifyTiles");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& tiles = ms_Resources->Get(TextureId::Sigma_Tiles);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ tiles, benzin::ResourceState::UnorderedAccess });

        {
            using Resources = joint::SigmaClassifyTilesResources;

            cmdList.SetComputeRootResource(+Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+Resources::Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra).GetSrv());
            cmdList.SetComputeRootResource(+Resources::OutTiles, tiles.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaClassifyTiles));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        BenzinProfile();
        BenzinGpuProfile("SmoothTiles");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& smoothTiles = ms_Resources->Get(TextureId::Sigma_SmoothTiles);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ smoothTiles, benzin::ResourceState::UnorderedAccess });

        {
            using Resources = joint::SigmaSmoothTilesResources;

            cmdList.SetComputeRootResource(+Resources::Tiles, ms_Resources->Get(TextureId::Sigma_Tiles).GetSrv());
            cmdList.SetComputeRootResource(+Resources::OutSmoothTiles, smoothTiles.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaSmoothTiles));
        cmdList.Dispatch({ m_Consts.TileCount.x, m_Consts.TileCount.y, 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunBlurPass() const
    {
        BenzinProfile();
        BenzinGpuProfile("Blur");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::UnorderedAccess });

        {
            using Resources = joint::SigmaBlurResources;

            cmdList.SetComputeRootResource(+Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(+Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Resources::Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra).GetSrv());
            cmdList.SetComputeRootResource(+Resources::OutPenumbra, penumbra1.GetUav());
            cmdList.SetComputeRootResource(+Resources::OutShadow, shadowTemp1.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaBlur));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunPostBlurPass(bool isEnabled) const
    {
        BenzinProfile();
        BenzinGpuProfile("PostBlur");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

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
                benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::CopySource });

            cmdList.CopyResource(penumbra2, penumbra1);
            cmdList.CopyResource(shadowTemp2, shadowTemp1);
        
            return;
        }

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ penumbra2, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp2, benzin::ResourceState::UnorderedAccess });

        {
            using Resources = joint::SigmaBlurResources;

            cmdList.SetComputeRootResource(+Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal).GetSrv());
            cmdList.SetComputeRootResource(+Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Resources::Penumbra, penumbra1.GetSrv());
            cmdList.SetComputeRootResource(+Resources::Shadow, shadowTemp1.GetSrv());
            cmdList.SetComputeRootResource(+Resources::OutPenumbra, penumbra2.GetUav());
            cmdList.SetComputeRootResource(+Resources::OutShadow, shadowTemp2.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaPostBlur));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass(bool isEnabled) const
    {
        BenzinProfile();
        BenzinGpuProfile("TemporalStabilization");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& shadowTemp2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);
        const auto& shadow = ms_Resources->Get(TextureId::Shadow);

        if (!isEnabled)
        {
            cmdList.CopyTextureRegion(shadow, shadow.CalcSubResourceIndex(0, 0), shadowTemp2, 0);
            return;
        }

        const auto& historyLength = ms_Resources->Get(TextureId::ShadowHistoryLength);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ shadow, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ historyLength, benzin::ResourceState::UnorderedAccess });

        {
            using Resources = joint::SigmaTemporalStabilizationResources;

            cmdList.SetComputeRootResource(+Resources::Mv, ms_Resources->Get(TextureId::Mv).GetSrv());
            cmdList.SetComputeRootResource(+Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth).GetSrv());
            cmdList.SetComputeRootResource(+Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles).GetSrv());
            cmdList.SetComputeRootResource(+Resources::Penumbra, ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2).GetSrv());
            cmdList.SetComputeRootResource(+Resources::Shadow, shadowTemp2.GetSrv());
            cmdList.SetComputeRootResource(+Resources::ShadowHistory, ms_Resources->GetPrev(TextureId::Shadow).GetSrv());
            cmdList.SetComputeRootResource(+Resources::HistoryLength, ms_Resources->GetPrev(TextureId::ShadowHistoryLength).GetSrv());
            cmdList.SetComputeRootResource(+Resources::OutShadow, shadow.GetUav());
            cmdList.SetComputeRootResource(+Resources::OutHistoryLength, historyLength.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaTemporalStabilization));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });
    }

}
