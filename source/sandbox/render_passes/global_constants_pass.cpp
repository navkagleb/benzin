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

namespace sandbox
{

    GlobalConstantsPass::GlobalConstantsPass(benzin::Device& device, benzin::Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {
        benzin::MakeUniquePtr(m_FrameConstantBuffer, *ms_Device, "FrameConstantBuffer");
    }

    void GlobalConstantsPass::OnUpdate(const benzin::TickTimer& tickTimer)
    {
        const float aspectRatio = (float)GetRenderViewportWidth() / GetRenderViewportHeight();
        const float pixelToWorldScale = std::tan(0.5f * m_Scene.GetPerspectiveProjection().GetVerticalFovInRadians()) / GetRenderViewportHeight(); // ViewToClip[1][1] factor

        UpdateCameraConstants();

        m_FrameConstantBuffer->UpdateConstants(joint::FrameConstants
        {
            .RenderResolution{ (float)GetRenderViewportWidth(), (float)GetRenderViewportHeight() },
            .InvRenderResolution{ 1.0f / (float)GetRenderViewportWidth(), 1.0f / (float)GetRenderViewportHeight() },
            .RenderAspectRatio = aspectRatio,
            .PixelToWorldScale = pixelToWorldScale,

            .CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex(),
            .FrameTimeInSec = tickTimer.GetDeltaTimeInSec(),
            .ElapsedTimeInSec = tickTimer.GetElapsedTimeInSec(),

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
            .WorldToViewForNormals = camera.GetWorldToViewMatrixForNormals(),
            .InvWorldToView = camera.GetInvWorldToViewMatrix(),

            .ViewToClip = camera.GetViewToClipMatrix(),
            .InvViewToClip = camera.GetInvViewToClipMatrix(),

            .WorldToClip = camera.GetWorldToClipMatrix(),
            .InvWorldToClip = camera.GetInvWorldToClipMatrix(),
            .InvDirectionWorldToClip = camera.GetInvDirectionalWorldToClipMatrix(),

            .WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition()),

            .PackedFrustumPlaneSlopes = projection.GetPackedFrustumPlaneSlopes(),
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
