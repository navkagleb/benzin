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
        ms_Resources->DestroyTexture(+Texture::SigmaHistory);
        ms_Resources->DestroyTexture(+Texture::SigmaPenumbra1);
        ms_Resources->DestroyTexture(+Texture::SigmaPenumbra2);
        ms_Resources->DestroyTexture(+Texture::SigmaShadowTemp1);
        ms_Resources->DestroyTexture(+Texture::SigmaShadowTemp2);
        ms_Resources->DestroyTexture(+Texture::SigmaShadow);
    }

    void SigmaDenoiserPass::OnRenderViewportResize()
    {
        m_TileCount.x = benzin::DivideUp(GetRenderViewportWidth(), joint::g_SigmaTileSize);
        m_TileCount.y = benzin::DivideUp(GetRenderViewportHeight(), joint::g_SigmaTileSize);

        ms_Resources->CreateTexture(+Texture::SigmaTiles, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::SigmaTiles),
            .Format = benzin::GraphicsFormat::Rgba8Unorm,
            .Width = m_TileCount.x,
            .Height = m_TileCount.y,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        ms_Resources->CreateTexture(+Texture::SigmaSmoothTiles, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::SigmaSmoothTiles),
            .Format = benzin::GraphicsFormat::Rg8Unorm,
            .Width = m_TileCount.x,
            .Height = m_TileCount.y,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        ms_Resources->CreateTexture(+Texture::SigmaHistory, benzin::TextureCreation
        {
            .DebugName = magic_enum::enum_name(Texture::SigmaHistory),
            .Format = benzin::GraphicsFormat::R8Unorm,
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        {
            const auto createSigmaTexture = [](Texture textureIndex, benzin::GraphicsFormat format)
            {
                ms_Resources->CreateTexture(+textureIndex, benzin::TextureCreation
                {
                    .DebugName = magic_enum::enum_name(textureIndex),
                    .Format = format,
                    .Width = GetRenderViewportWidth(),
                    .Height = GetRenderViewportHeight(),
                    .MipCount = 1,
                    .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
                });
            };

            createSigmaTexture(Texture::SigmaPenumbra1, benzin::GraphicsFormat::R32Float);
            createSigmaTexture(Texture::SigmaPenumbra2, benzin::GraphicsFormat::R32Float);

            createSigmaTexture(Texture::SigmaShadowTemp1, benzin::GraphicsFormat::R8Unorm);
            createSigmaTexture(Texture::SigmaShadowTemp2, benzin::GraphicsFormat::R8Unorm);
            createSigmaTexture(Texture::SigmaShadow, benzin::GraphicsFormat::R8Unorm);
        }
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

        RunClassifyTilesPass();
        RunSmoothTilesPass();
        RunBlurPass();
        RunPostBlurPass();
        RunTemporalStabilizationPass();
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
        const auto& history = ms_Resources->GetTexture(+Texture::SigmaHistory);
        const auto& shadowTemp1 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp1);

        {
            using enum joint::Rc_SigmaBlur;

            commandList.SetRootResource(+WorldNormalTex, ms_Resources->GetTexture(+Texture::WorldNormal).GetSrv());
            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::NoisyPenumbra).GetSrv());
            commandList.SetRootResource(+SmoothTilesTex, ms_Resources->GetTexture(+Texture::SigmaSmoothTiles).GetSrv());
            commandList.SetRootResource(+HistoryTex, ms_Resources->GetTexture(+Texture::SigmaShadow).GetSrv());
            commandList.SetRootResource(+OutHistoryTex, history.GetUav());
            commandList.SetRootResource(+OutPenumbraTex, penumbra1.GetUav());
            commandList.SetRootResource(+OutShadowTex, shadowTemp1.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            commandList,
            benzin::TransitionBarrier{ history, benzin::ResourceState::UnorderedAccess },
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

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsPostBlurEnabled)
        {
            return;
        }

        commandList.SetPipelineState(*m_PostBlurPso);

        const auto& penumbra2 = ms_Resources->GetTexture(+Texture::SigmaPenumbra2);
        const auto& shadowTemp2 = ms_Resources->GetTexture(+Texture::SigmaShadowTemp2);

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

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();
        if (!settings.IsTemporalStabilizationEnabled)
        {
            return;
        }

        commandList.SetPipelineState(*m_TemporalStabilizationPso);

        const auto& shadow = ms_Resources->GetTexture(+Texture::SigmaShadow);

        {
            using enum joint::Rc_SigmaTemporalStabilization;

            commandList.SetRootResource(+ViewDepthTex, ms_Resources->GetTexture(+Texture::ViewDepth).GetSrv());
            commandList.SetRootResource(+MvTex, ms_Resources->GetTexture(+Texture::VelocityBuffer).GetSrv());
            commandList.SetRootResource(+PenumbraTex, ms_Resources->GetTexture(+Texture::SigmaPenumbra2).GetSrv());
            commandList.SetRootResource(+ShadowTex, ms_Resources->GetTexture(+Texture::SigmaShadowTemp2).GetSrv());
            commandList.SetRootResource(+HistoryTex, ms_Resources->GetTexture(+Texture::SigmaHistory).GetSrv());
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
