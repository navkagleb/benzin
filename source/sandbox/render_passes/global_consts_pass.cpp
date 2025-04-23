#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/global_consts_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>

#include <sandbox/sandbox_render_settings.hpp>

BenzinEnableUnaryPlusForEnum(joint::FrustumPlane);

namespace sandbox
{

    GlobalConstsPass::GlobalConstsPass()
    {
        ms_ConstBufferPool->PreAllocate(sizeof(m_FrameConsts));
    }

    void GlobalConstsPass::OnUpdate()
    {
        BenzinProfile();

        UpdateCameraConsts();
        UpdateFrameConsts();
    }

    void GlobalConstsPass::OnRender() const
    {
        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuEvent(cmdList, "GlobalConstsPass");

        const uint64_t frameConstsGpuAddress = ms_ConstBufferPool->Allocate(m_FrameConsts);
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);
        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);

        cmdList.SetComputeSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, ms_Scene->GetLightBufferGpuAddress());
        cmdList.SetGraphicsSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, ms_Scene->GetLightBufferGpuAddress());
    }

    void GlobalConstsPass::UpdateCameraConsts()
    {
        const benzin::Camera& camera = ms_Scene->GetCamera();
        const benzin::PerspectiveProjection& projection = ms_Scene->GetPerspectiveProjection();

        joint::CameraConsts cameraConsts
        {
            .WorldToView = camera.GetWorldToViewMatrix(),
            .ViewToWorld = camera.GetViewToWorldMatrix(),

            .ViewToClip = camera.GetViewToClipMatrix(),
            .ClipToView = camera.GetClipToViewMatrix(),

            .WorldToClip = camera.GetWorldToClipMatrix(),
            .ClipToWorld = camera.GetClipToWorldMatrix(),
            .ClipToWorldNoTranslation = camera.GetClipToWorldNoTranslation(),

            .WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),
            .PixelToWorldScale = projection.GetPixelToWorldScale(GetRenderViewportHeight()),

            .UvToViewScale = projection.GetUvToViewScale(),
            .UvToViewBias = projection.GetUvToViewBias(),
        };

        camera.GetWorldFrustum().GetPlanes(
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Near],
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Far],
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Right],
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Left],
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Top],
            (DirectX::XMVECTOR*)&cameraConsts.WorldFrustumPlanes[+joint::FrustumPlane::Bottom]
        );

        if (ms_Device->GetCpuFrameIndex() != 0) [[likely]]
        {
            m_FrameConsts.PrevCamera = std::exchange(m_FrameConsts.Camera, cameraConsts);
        }
        else
        {
            m_FrameConsts.Camera = cameraConsts;
            m_FrameConsts.PrevCamera = cameraConsts;
        }
    }

    void GlobalConstsPass::UpdateFrameConsts()
    {
        const DirectX::XMUINT2 renderResolution{ GetRenderViewportWidth(), GetRenderViewportHeight() };
        const float animationTimeInSec = ms_AnimationTimer->GetElapsedTimeInSec();

        m_FrameConsts.RenderResolution = { (float)renderResolution.x, (float)renderResolution.y };
        m_FrameConsts.InvRenderResolution = { 1.0f / (float)renderResolution.x, 1.0f / (float)renderResolution.y };
        m_FrameConsts.MinRenderDimension = (float)std::min(renderResolution.x, renderResolution.y);

        m_FrameConsts.CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex();
        m_FrameConsts.LightCount = ms_Scene->GetActiveLightCount();

        m_FrameConsts.IsRenderResolutionChanged = renderResolution.x != m_PrevRenderResolution.x || renderResolution.y != m_PrevRenderResolution.y;
        m_FrameConsts.IsShadowsEnabled = ms_Settings->GetSection<RayTracing_ShadowSettings>().IsEnabled;
        m_FrameConsts.IsDenoiserEnabled = ms_Settings->GetSection<SigmaDenoiserSettings>().IsEnabled;

        m_FrameConsts.DeltaTimeInSec = ms_FrameTimer->GetDeltaTimeInSec();
        m_FrameConsts.AnimationElapsedTimeInSec = animationTimeInSec;
        m_FrameConsts.PrevAnimationElapsedTimeInSec = m_PrevAnimationElapsedTimeInSec;

        m_PrevRenderResolution = renderResolution;
        m_PrevAnimationElapsedTimeInSec = animationTimeInSec;
    }

}
