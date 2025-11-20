#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/sigma_denoiser_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::SigmaClassifyTilesResources);
BenzinAllowDereferenceOperatorForEnum(joint::SigmaSmoothTilesResources);
BenzinAllowDereferenceOperatorForEnum(joint::SigmaBlurResources);
BenzinAllowDereferenceOperatorForEnum(joint::SigmaTemporalStabilizationResources);

namespace sandbox
{

    static float GetWeylSequence(float seed, uint32_t n)
    {
        // Ref: https://extremelearning.com.au/unreasonable-effectiveness-of-quasirandom-sequences/
        // [0, 1)

        float integerPart;
        return std::modf(seed + (float)(n * 10368889) / std::exp2(24.0f), &integerPart);
    }

    static DirectX::XMFLOAT4 GetRotator(float angleInRadians)
    {
        // Ref: https://en.wikipedia.org/wiki/Rotation_matrix
        // This is 2x2 rotation matrix

        const float cosAngle = DirectX::XMScalarCos(angleInRadians);
        const float sinAngle = DirectX::XMScalarSin(angleInRadians);

        // TODO: Do I need to transpose it?
        return DirectX::XMFLOAT4{ cosAngle, sinAngle, -sinAngle, cosAngle };
    }

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
                proxy.m_Cs.m_FileName = fileName;

                if (!define.empty())
                {
                    proxy.m_Cs.m_Defines.push_back(define);
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

        const auto createTexture = [](TextureId id, DXGI_FORMAT dxgiFormat, DirectX::XMUINT2 resolution)
        {
            ms_Resources->Create(id, benzin::TextureCreation
            {
                .m_DebugName = magic_enum::enum_name(id),
                .m_DxgiFormat = dxgiFormat,
                .m_Width = resolution.x,
                .m_Height = resolution.y,
                .m_MipCount = 1,
                .m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
            });
        };

        const DirectX::XMUINT2 renderResolution{ ms_RenderViewportWidth, ms_RenderViewportHeight };
        const DXGI_FORMAT shadowFormat = DXGI_FORMAT_R8_UNORM;
        const DXGI_FORMAT penumbraFormat = SigmaDenoiserSettings::ms_PenumbraDxgiFormat;

        createTexture(TextureId::Sigma_Tiles, DXGI_FORMAT_R8G8B8A8_UNORM, m_Consts.TileCount);
        createTexture(TextureId::Sigma_SmoothTiles, DXGI_FORMAT_R8G8_UNORM, m_Consts.TileCount);
        createTexture(TextureId::Sigma_BlurredPenumbra1, penumbraFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredPenumbra2, penumbraFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredShadowTemp1, shadowFormat, renderResolution);
        createTexture(TextureId::Sigma_BlurredShadowTemp2, shadowFormat, renderResolution);
        createTexture(TextureId::Shadow, shadowFormat, renderResolution);
        createTexture(TextureId::ShadowHistoryLength, DXGI_FORMAT_R32_UINT, renderResolution);
    }

    void SigmaDenoiserPass::OnUpdate()
    {
        auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        m_IsRenderingEnabled = settings.m_IsEnabled;
        if (!m_IsRenderingEnabled)
            return;

        // TODO: Do I need cast to u32?
        const float rotatorAngleInRadians = GetWeylSequence(0.0f, (uint32_t)ms_Device->GetCpuFrameIndex()) * DirectX::XMConvertToRadians(90.0f);
        const DirectX::XMFLOAT4 blurRotator = GetRotator(rotatorAngleInRadians);
        const DirectX::XMFLOAT4 postBlurRotator = GetRotator(rotatorAngleInRadians + DirectX::XMConvertToRadians(45.0f));

        const float fps = 1.0f / ms_FrameTimer->GetDeltaTimeInSec();
        settings.m_HistoryLength = GetMaxHistoryLength(SigmaDenoiserSettings::ms_MaxHistoryLength, fps);
        settings.m_StabilizationStrength = settings.m_HistoryLength / (1.0f + settings.m_HistoryLength);

        m_Consts.BlurRotator = blurRotator;
        m_Consts.PostBlurRotator = postBlurRotator;
        m_Consts.StabilizationStrength = settings.m_StabilizationStrength;
        m_Consts.PlaneDistanceSensitivity = settings.m_PlaneDistanceSensitivity;
        m_Consts.DisocclusionThreshold = settings.m_DisocclusionThreshold;
        m_Consts.IsTileSmoothingEnabled = settings.m_IsTileSmoothingEnabled;
    }

    void SigmaDenoiserPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("SigmaDenoiser");

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        const auto& settings = ms_Settings->GetSection<SigmaDenoiserSettings>();

        RunClearPass(settings.m_IsClearEnabled);
        RunClassifyTilesPass();
        RunSmoothTilesPass();
        RunBlurPass();
        RunPostBlurPass(settings.m_IsPostBlurEnabled);
        RunTemporalStabilizationPass(settings.m_IsTemporalStabilizationEnabled);
    }

    void SigmaDenoiserPass::RunClearPass(bool isEnabled) const
    {
        BenzinProfile();
        BenzinGpuProfile("Clear");

        if (!isEnabled)
            return;

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const auto& tiles = ms_Resources->Get(TextureId::Sigma_Tiles);
        const auto& smoothTiles = ms_Resources->Get(TextureId::Sigma_SmoothTiles);
        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& penumbra2 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2);
        const auto& shadow1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);
        const auto& shadow2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);

        cmdList.AddTransition(tiles, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(smoothTiles, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(penumbra1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(penumbra2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(shadow1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(shadow2, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.FlushBarriers();

        cmdList.ClearUnorderedAccess(tiles, tiles.GetUav());
        cmdList.ClearUnorderedAccess(smoothTiles, smoothTiles.GetUav());
        cmdList.ClearUnorderedAccess(penumbra1, penumbra1.GetUav());
        cmdList.ClearUnorderedAccess(penumbra2, penumbra2.GetUav());
        cmdList.ClearUnorderedAccess(shadow1, shadow1.GetUav());
        cmdList.ClearUnorderedAccess(shadow2, shadow2.GetUav());

        cmdList.AddUnorderedAccess(tiles);
        cmdList.AddUnorderedAccess(smoothTiles);
        cmdList.AddUnorderedAccess(penumbra1);
        cmdList.AddUnorderedAccess(penumbra2);
        cmdList.AddUnorderedAccess(shadow1);
        cmdList.AddUnorderedAccess(shadow2);
    }

    void SigmaDenoiserPass::RunClassifyTilesPass() const
    {
        using Resources = joint::SigmaClassifyTilesResources;

        BenzinProfile();
        BenzinGpuProfile("ClassifyTiles");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        const auto& tiles = ms_Resources->Get(TextureId::Sigma_Tiles);

        cmdList.SetComputeRootSrv(*Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth));
        cmdList.SetComputeRootSrv(*Resources::Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra));
        cmdList.SetComputeRootUav(*Resources::OutTiles, tiles);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaClassifyTiles));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });

        cmdList.AddUnorderedAccess(tiles);
    }

    void SigmaDenoiserPass::RunSmoothTilesPass() const
    {
        using Resources = joint::SigmaSmoothTilesResources;

        BenzinProfile();
        BenzinGpuProfile("SmoothTiles");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        const auto& smoothTiles = ms_Resources->Get(TextureId::Sigma_SmoothTiles);

        cmdList.SetComputeRootSrv(*Resources::Tiles, ms_Resources->Get(TextureId::Sigma_Tiles));
        cmdList.SetComputeRootUav(*Resources::OutSmoothTiles, smoothTiles);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaSmoothTiles));
        cmdList.Dispatch({ m_Consts.TileCount.x, m_Consts.TileCount.y, 1 }, { 16, 16, 1 });

        cmdList.AddUnorderedAccess(smoothTiles);
    }

    void SigmaDenoiserPass::RunBlurPass() const
    {
        using Resources = joint::SigmaBlurResources;

        BenzinProfile();
        BenzinGpuProfile("Blur");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);

        cmdList.SetComputeRootSrv(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal));
        cmdList.SetComputeRootSrv(*Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth));
        cmdList.SetComputeRootSrv(*Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles));
        cmdList.SetComputeRootSrv(*Resources::Penumbra, ms_Resources->Get(TextureId::NoisyPenumbra));
        cmdList.SetComputeRootUav(*Resources::OutPenumbra, penumbra1);
        cmdList.SetComputeRootUav(*Resources::OutShadow, shadowTemp1);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaBlur));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });

        cmdList.AddUnorderedAccess(penumbra1);
        cmdList.AddUnorderedAccess(shadowTemp1);
    }

    void SigmaDenoiserPass::RunPostBlurPass(bool isEnabled) const
    {
        using Resources = joint::SigmaBlurResources;

        BenzinProfile();
        BenzinGpuProfile("PostBlur");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        const auto& penumbra1 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra1);
        const auto& shadowTemp1 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp1);
        const auto& penumbra2 = ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2);
        const auto& shadowTemp2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);

        if (!isEnabled)
        {
            cmdList.CopyResource(penumbra2, penumbra1);
            cmdList.CopyResource(shadowTemp2, shadowTemp1);
            return;
        }

        cmdList.SetComputeRootSrv(*Resources::WorldNormal, ms_Resources->Get(TextureId::WorldNormal));
        cmdList.SetComputeRootSrv(*Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth));
        cmdList.SetComputeRootSrv(*Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles));
        cmdList.SetComputeRootSrv(*Resources::Penumbra, penumbra1);
        cmdList.SetComputeRootSrv(*Resources::Shadow, shadowTemp1);
        cmdList.SetComputeRootUav(*Resources::OutPenumbra, penumbra2);
        cmdList.SetComputeRootUav(*Resources::OutShadow, shadowTemp2);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaPostBlur));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });

        cmdList.AddUnorderedAccess(penumbra2);
        cmdList.AddUnorderedAccess(shadowTemp2);
    }

    void SigmaDenoiserPass::RunTemporalStabilizationPass(bool isEnabled) const
    {
        using Resources = joint::SigmaTemporalStabilizationResources;

        BenzinProfile();
        BenzinGpuProfile("TemporalStabilization");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        const auto& shadowTemp2 = ms_Resources->Get(TextureId::Sigma_BlurredShadowTemp2);
        const auto& shadow = ms_Resources->Get(TextureId::Shadow);
        const auto& historyLength = ms_Resources->Get(TextureId::ShadowHistoryLength);

        if (!isEnabled)
        {
            cmdList.CopyTextureRegion(shadow, shadow.CalcSubResourceIndex(0, 0), shadowTemp2, 0);
            return;
        }

        cmdList.SetComputeRootSrv(*Resources::Mv, ms_Resources->Get(TextureId::Mv));
        cmdList.SetComputeRootSrv(*Resources::ViewDepth, ms_Resources->Get(TextureId::ViewDepth));
        cmdList.SetComputeRootSrv(*Resources::SmoothTiles, ms_Resources->Get(TextureId::Sigma_SmoothTiles));
        cmdList.SetComputeRootSrv(*Resources::Penumbra, ms_Resources->Get(TextureId::Sigma_BlurredPenumbra2));
        cmdList.SetComputeRootSrv(*Resources::Shadow, shadowTemp2);
        cmdList.SetComputeRootSrv(*Resources::ShadowHistory, ms_Resources->GetPrev(TextureId::Shadow));
        cmdList.SetComputeRootSrv(*Resources::HistoryLength, ms_Resources->GetPrev(TextureId::ShadowHistoryLength));
        cmdList.SetComputeRootUav(*Resources::OutShadow, shadow);
        cmdList.SetComputeRootUav(*Resources::OutHistoryLength, historyLength);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::SigmaTemporalStabilization));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 8, 16, 1 });

        cmdList.AddUnorderedAccess(shadow);
        cmdList.AddUnorderedAccess(historyLength);
    }

}
