#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/global_constants_pass.hpp"

#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/rt_acceleration_structures.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/utility/random.hpp>

#include "sandbox/sandbox_render_settings.hpp"

namespace sandbox
{

    float CalcPixelToWorldScale(const DirectX::XMMATRIX& viewToClip, uint32_t viewportHeight)
    {
        // viewToClip[1][1] = 1.0f / std::tan(0.5f * verticalFov)

        const float projectionScaleY = DirectX::XMVectorGetByIndex(viewToClip.r[1], 1);
        const float pixelToWorldScale = 2.0f / ((float)viewportHeight * projectionScaleY);

        return pixelToWorldScale;
    }

    //

    GlobalConstantsPass::GlobalConstantsPass(benzin::Device& device, benzin::Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {
        benzin::MakeUniquePtr(m_FrameConstantBuffer, *ms_Device, "FrameConstantBuffer");
    }

    void GlobalConstantsPass::OnUpdate()
    {
        UpdateCameraConstants();

        const float pixelToWorldScale = CalcPixelToWorldScale(m_Scene.GetPerspectiveProjection().GetViewToClipMatrix(), GetRenderViewportHeight());

        m_FrameConstantBuffer->UpdateConstants(joint::FrameConstants
        {
            .RenderResolution{ (float)GetRenderViewportWidth(), (float)GetRenderViewportHeight() },
            .InvRenderResolution{ 1.0f / (float)GetRenderViewportWidth(), 1.0f / (float)GetRenderViewportHeight() },
            
            .PixelToWorldScale = pixelToWorldScale,
            .CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex(),

            .IsShadowsEnabled = ms_Settings->GetSection<RayTracingShadowsSettings>().IsEnabled,
            .IsDenoiserEnabled = ms_Settings->GetSection<SigmaDenoiserSettings>().IsEnabled,

            .RandomFloats01
            {
                benzin::Random::Get<float>(0.0f, 1.0f),
                benzin::Random::Get<float>(0.0f, 1.0f),
                benzin::Random::Get<float>(0.0f, 1.0f),
                benzin::Random::Get<float>(0.0f, 1.0f),
            },

            .Camera = m_CameraConstants,
            .PrevCamera = m_PrevCameraConstants,
        });
    }

    void GlobalConstantsPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "GlobalConstantsPass");

        commandList.SetCbv(benzin::UnifiedRootParameter::FrameConstantBuffer, m_FrameConstantBuffer->GetActiveGpuVirtualAddress());

        if (m_Scene.HasMeshes())
        {
            commandList.SetSrv(benzin::UnifiedRootParameter::TopLevelAs, m_Scene.GetActiveTopLevelAs().GetBuffer().GetGpuVirtualAddress());
        }
    }

    void GlobalConstantsPass::UpdateCameraConstants()
    {
        const auto& camera = m_Scene.GetCamera();
        const auto& projection = m_Scene.GetPerspectiveProjection();

        const joint::CameraConstants cameraConstants
        {
            .WorldToView = camera.GetWorldToViewMatrix(),
            .ViewToWorld = camera.GetViewToWorldMatrix(),

            .ViewToClip = camera.GetViewToClipMatrix(),
            .ClipToView = camera.GetClipToViewMatrix(),

            .WorldToClip = camera.GetWorldToClipMatrix(),
            .ClipToWorld = camera.GetClipToWorldMatrix(),
            .ClipToWorldNoTranslation = camera.GetClipToWorldNoTranslation(),

            .WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),

            .UvToViewScale = projection.GetUvToViewScale(),
            .UvToViewBias = projection.GetUvToViewBias(),
        };

        if (m_Device.GetCpuFrameIndex() != 0)
        {
            m_PrevCameraConstants = std::exchange(m_CameraConstants, cameraConstants);
        }
        else
        {
            m_PrevCameraConstants = cameraConstants;
            m_CameraConstants = cameraConstants;
        }
    }

}
