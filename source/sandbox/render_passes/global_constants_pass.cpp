#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/global_constants_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>

#include <sandbox/sandbox_render_settings.hpp>

namespace sandbox
{

    GlobalConstantsPass::GlobalConstantsPass()
    {
        ms_ConstBufferPool->PreAllocate(sizeof(m_FrameConsts));
    }

    void GlobalConstantsPass::OnUpdate()
    {
        BenzinProfile();

        UpdateCameraConsts();
        UpdateFrameConsts();
    }

    void GlobalConstantsPass::OnRender() const
    {
        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        BenzinGpuEvent(cmdList, "GlobalConstants");

        const DirectX::XMUINT2 renderResolution{ GetRenderViewportWidth(), GetRenderViewportHeight() };

        const uint64_t frameConstsGpuAddress = ms_ConstBufferPool->Allocate(m_FrameConsts);
        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);
        cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);

        cmdList.SetComputeSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, ms_Scene->GetLightBufferGpuAddress());
        cmdList.SetGraphicsSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, ms_Scene->GetLightBufferGpuAddress());
    }

    void GlobalConstantsPass::UpdateCameraConsts()
    {
        const benzin::Camera& camera = ms_Scene->GetCamera();
        const benzin::PerspectiveProjection& projection = ms_Scene->GetPerspectiveProjection();

        const joint::CameraConsts cameraConstants
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

        if (ms_Device->GetCpuFrameIndex() != 0)
        {
            m_FrameConsts.PrevCamera = std::exchange(m_FrameConsts.Camera, cameraConstants);
        }
        else
        {
            m_FrameConsts.Camera = cameraConstants;
            m_FrameConsts.PrevCamera = cameraConstants;
        }
    }

    void GlobalConstantsPass::UpdateFrameConsts()
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
