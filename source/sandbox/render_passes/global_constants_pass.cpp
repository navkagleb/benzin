#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/global_constants_pass.hpp"

#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/utility/random.hpp>

#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    GlobalConstantsPass::GlobalConstantsPass(benzin::Device& device, const benzin::Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {
        ms_ConstBufferPool->PreAllocate<joint::FrameConsts>();
    }

    GlobalConstantsPass::~GlobalConstantsPass() = default;

    void GlobalConstantsPass::OnUpdate()
    {
        UpdateCameraConsts();
        UpdateFrameConsts();
    }

    void GlobalConstantsPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();

        BenzinGpuEvent(commandList, "GlobalConstants");

        const DirectX::XMUINT2 renderResolution{ GetRenderViewportWidth(), GetRenderViewportHeight() };

        commandList.SetCbv(benzin::UnifiedRootParameter::FrameConstantBuffer, ms_ConstBufferPool->Allocate(m_FrameConsts));
        commandList.SetSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, m_Scene.GetLightBufferGpuAddress());
    }

    void GlobalConstantsPass::UpdateCameraConsts()
    {
        const auto& camera = m_Scene.GetCamera();
        const auto& projection = m_Scene.GetPerspectiveProjection();

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

        if (m_Device.GetCpuFrameIndex() != 0)
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

        m_FrameConsts.RenderResolution = { (float)renderResolution.x, (float)renderResolution.y };
        m_FrameConsts.InvRenderResolution = { 1.0f / (float)renderResolution.x, 1.0f / (float)renderResolution.y };
        m_FrameConsts.MinRenderDimension = (float)std::min(renderResolution.x, renderResolution.y);

        m_FrameConsts.CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex();
        m_FrameConsts.LightCount = m_Scene.GetActiveLightCount();

        m_FrameConsts.IsRenderResolutionChanged = renderResolution.x != m_PrevRenderResolution.x || renderResolution.y != m_PrevRenderResolution.y;
        m_FrameConsts.IsShadowsEnabled = ms_Settings->GetSection<RayTracing_ShadowSettings>().IsEnabled;
        m_FrameConsts.IsDenoiserEnabled = ms_Settings->GetSection<SigmaDenoiserSettings>().IsEnabled;
        m_FrameConsts.RandomFloats01 =
        {
            benzin::Random::Get<float>(0.0f, 1.0f),
            benzin::Random::Get<float>(0.0f, 1.0f),
            benzin::Random::Get<float>(0.0f, 1.0f),
            benzin::Random::Get<float>(0.0f, 1.0f),
        };

        m_PrevRenderResolution = renderResolution;
    }

}
