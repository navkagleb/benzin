#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/sigma_denoiser_pass.hpp"

#include <benzin/core/engine_math.hpp>
#include <benzin/core/math.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/entity_components.hpp>
#include <benzin/engine/light.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include "sandbox/sandbox_render_settings.hpp"
#include "sandbox/resources.hpp"

BenzinEnableUnaryPlusForEnum(joint::Rc_SigmaClassifyTiles);
BenzinEnableUnaryPlusForEnum(joint::Rc_SigmaSmoothTiles);
BenzinEnableUnaryPlusForEnum(joint::Rc_SigmaBlur);
BenzinEnableUnaryPlusForEnum(joint::Rc_SigmaTemporalStabilization)

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

    SigmaDenoiserPass::SigmaDenoiserPass(const benzin::Scene& scene)
        : m_Scene{ scene }
    {
        const auto createPso = [](Pso psoIndex, std::string_view fileName, std::string_view define = {})
        {
            const std::string_view debugName = magic_enum::enum_name(psoIndex);

            ms_PsoManager->CreateComputePso(+psoIndex, [debugName, fileName, define](benzin::ComputePsoProxy& proxy)
            {
                proxy.DebugName = debugName;
                proxy.CsFileName = fileName;

                if (!define.empty())
                {
                    proxy.CsDefines.push_back(define);
                }
            });
        };

        createPso(Pso::SigmaClassifyTiles, "sigma_denoiser/classify_tiles.hlsl");
        createPso(Pso::SigmaSmoothTiles, "sigma_denoiser/smooth_tiles.hlsl");
        createPso(Pso::SigmaBlur, "sigma_denoiser/blur.hlsl");
        createPso(Pso::SigmaPostBlur, "sigma_denoiser/blur.hlsl", "POST_BLUR_PASS");
        createPso(Pso::SigmaTemporalStabilization, "sigma_denoiser/temporal_stabilization.hlsl");

        ms_ConstBufferPool->PreAllocate<joint::SigmaConsts>();

        m_PerLightConsts.resize(benzin::Scene::s_MaxLightCount);
        for (uint32_t i = 0; i < m_PerLightConsts.size(); ++i)
        {
            ms_ConstBufferPool->PreAllocate<joint::SigmaPerLightConsts>();
        }
    }

    SigmaDenoiserPass::~SigmaDenoiserPass()
    {
        ms_PsoManager->DestroyPso(+Pso::SigmaClassifyTiles);
        ms_PsoManager->DestroyPso(+Pso::SigmaSmoothTiles);
        ms_PsoManager->DestroyPso(+Pso::SigmaBlur);
        ms_PsoManager->DestroyPso(+Pso::SigmaPostBlur);
        ms_PsoManager->DestroyPso(+Pso::SigmaTemporalStabilization);

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
        m_Consts.TileCount.x = benzin::DivideUp(GetRenderViewportWidth(), joint::g_SigmaTileSize);
        m_Consts.TileCount.y = benzin::DivideUp(GetRenderViewportHeight(), joint::g_SigmaTileSize);

        const DirectX::XMUINT2 renderResolution = GetRenderResolution();
        const auto shadowFormat = benzin::GraphicsFormat::R8Unorm;

        const auto createTexture = [](Texture texture, benzin::GraphicsFormat format, DirectX::XMUINT2 resolution, uint16_t depth = 1)
        {
            ms_Resources->CreateTexture(+texture, benzin::TextureCreation
            {
                .DebugName = magic_enum::enum_name(texture),
                .Format = format,
                .Width = resolution.x,
                .Height = resolution.y,
                .Depth = depth,
                .MipCount = 1,
                .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
            });
        };

        const auto penumbraFormat = ms_Settings->GetSection<SigmaDenoiserSettings>().PenumbraFormat;

        createTexture(Texture::Sigma_Tiles, benzin::GraphicsFormat::Rgba8Unorm, m_Consts.TileCount);
        createTexture(Texture::Sigma_SmoothTiles, benzin::GraphicsFormat::Rg8Unorm, m_Consts.TileCount);
        createTexture(Texture::Sigma_BlurredPenumbra1, penumbraFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredPenumbra2, penumbraFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredShadowTemp1, shadowFormat, renderResolution);
        createTexture(Texture::Sigma_BlurredShadowTemp2, shadowFormat, renderResolution);
        createTexture(Texture::Shadow, shadowFormat, renderResolution, benzin::Scene::s_MaxLightCount);
        createTexture(Texture::ShadowHistoryLength, benzin::GraphicsFormat::R32Uint, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate(const benzin::TickTimer& tickTimer)
    {
        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = benzin::GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = benzin::GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = benzin::GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        const float fps = 1.0f / tickTimer.GetDeltaTimeInSec();
        settings.HistoryLength = GetMaxHistoryLength(settings.MaxHistoryLength, fps);
        settings.StabilizationStrength = settings.HistoryLength / (1.0f + settings.HistoryLength);

        m_Consts.BlurRotator = blurRotator;
        m_Consts.PostBlurRotator = postBlurRotator;
        m_Consts.StabilizationStrength = settings.StabilizationStrength;
        m_Consts.PlaneDistanceSensitivity = settings.PlaneDistanceSensitivity;
        m_Consts.DisocclusionThreshold = settings.DisocclusionThreshold;
        m_Consts.IsTileSmoothingEnabled = settings.IsTileSmoothingEnabled;

        {
            const auto& entityRegistry = m_Scene.GetEntityRegistry();
            uint32_t lightOffset = 0;

            {
                const auto& light = entityRegistry.get<benzin::SunLight>(m_Scene.GetSunEntity());

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
        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsEnabled)
        {
            return;
        }

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));
        BenzinGpuEvent(commandList, "SigmaDenoiser");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "SigmaDenoiser");

        const uint32_t lightCount = m_Scene.GetActiveLightCount();
        for (uint16_t sliceIndex = 0; sliceIndex < lightCount; ++sliceIndex)
        {
            if (lightCount != 1)
            {
                BenzinGpuEvent(commandList, "Step");
            }

            commandList.SetCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer1, ms_ConstBufferPool->Allocate(m_PerLightConsts[sliceIndex]));

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

    void SigmaDenoiserPass::RunClassifyTilesPass(uint16_t sliceIndex) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        const auto& tiles = ms_Resources->GetTexture(+Texture::Sigma_Tiles);
        BenzinGpuEvent(commandList, "ClassifyTiles");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "ClassifyTiles");

        {
            using enum joint::Rc_SigmaClassifyTiles;

            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv({ .DepthRange = sliceIndex }));

            commandList.SetRootResource(+OutTiles, tiles.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ tiles, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::SigmaClassifyTiles));
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        const auto& smoothTiles = ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles);
        BenzinGpuEvent(commandList, "SmoothTiles");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "SmoothTiles");

        {
            using enum joint::Rc_SigmaSmoothTiles;

            commandList.SetRootResource(+Tiles, ms_Resources->GetTexture(+Texture::Sigma_Tiles).GetSrv());
            commandList.SetRootResource(+OutSmoothTiles, smoothTiles.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ smoothTiles, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::SigmaSmoothTiles));
        commandList.Dispatch({ m_Consts.TileCount.x, m_Consts.TileCount.y, 1 }, { 16, 16, 1 });
    }

    void SigmaDenoiserPass::RunBlurPass(uint16_t sliceIndex) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1);
        BenzinGpuEvent(commandList, "Blur");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "Blur");

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormal, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+SmoothTiles, ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv({ .DepthRange = sliceIndex }));

            commandList.SetRootResource(+OutPenumbra, penumbra1.GetUav());
            commandList.SetRootResource(+OutShadow, shadowTemp1.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ penumbra1, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ shadowTemp1, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::SigmaBlur));
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunPostBlurPass(bool isEnabled) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        const auto& penumbra1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra1);
        const auto& penumbra2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp1);
        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2);
        BenzinGpuEvent(commandList, "PostBlur");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "PostBlur");

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

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::SigmaPostBlur));
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass(bool isEnabled, uint16_t sliceIndex) const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::Sigma_BlurredShadowTemp2);
        const auto& shadow = ms_Resources->GetTexture(+Texture::Shadow);
        BenzinGpuEvent(commandList, "TemporalStabilization");
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "TemporalStabilization");

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

            commandList.SetRootResource(+Mv, ms_Resources->GetTexture(+Texture::Mv).GetSrv());
            commandList.SetRootResource(+ViewDepth, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+SmoothTiles, ms_Resources->GetTexture(+Texture::Sigma_SmoothTiles).GetSrv());
            commandList.SetRootResource(+Penumbra, ms_Resources->GetTexture(+Texture::Sigma_BlurredPenumbra2).GetSrv());
            commandList.SetRootResource(+Shadow, shadowTemp2.GetSrv());
            commandList.SetRootResource(+ShadowHistory, ms_Resources->GetPrevTexture(+Texture::Shadow).GetSrv({ .DepthRange = sliceIndex }));
            commandList.SetRootResource(+HistoryLength, ms_Resources->GetPrevTexture(+Texture::ShadowHistoryLength).GetSrv());

            commandList.SetRootResource(+OutShadow, shadow.GetUav({ .DepthRange = sliceIndex }));
            commandList.SetRootResource(+OutHistoryLength, historyLength.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ shadow, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ historyLength, benzin::ResourceState::UnorderedAccess },
        );

        commandList.SetPso(ms_PsoManager->GetPso(+Pso::SigmaTemporalStabilization));
        commandList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 8, 16, 1 });
    }

}
