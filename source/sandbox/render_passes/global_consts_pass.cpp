#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>

#include <sandbox/render_settings.hpp>

#include <benzin/core/math.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::FrustumPlane);

namespace sandbox
{

    static DirectX::XMFLOAT3 CalcToSunDirection(const benzin::SunLight& sun)
    {
        const float pitch = sun.m_ElevationInRadians;
        const float yaw = sun.m_AzimuthInRadians;

        DirectX::XMVECTOR sunDirection = benzin::GetDirectionFromPitchYaw(pitch, yaw); // sunDirection vector directed towards the sun

        DirectX::XMFLOAT3 sunDirection3 = {};
        DirectX::XMStoreFloat3(&sunDirection3, sunDirection);

        return sunDirection3;
    }

    //

    GlobalConstsPass::GlobalConstsPass(ReadbackStatsCallback&& callback)
        : m_ReadbackStatsCallback{ std::move(callback) }
    {
        BenzinAssert(m_ReadbackStatsCallback);

        constexpr DXGI_FORMAT statFormat = DXGI_FORMAT_R32_UINT;
        constexpr uint64_t statElementCount = magic_enum::enum_count<joint::ReadbackStat>();
        const uint32_t statElementSizeInBytes = benzin::GetDxgiFormatSizeInBytes(statFormat);

        // Usage as ByteAddressBuffer
        m_StatBuffer = ms_Device->GetPersistentDefaultAllocator().AllocateBuffer([&](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GlobalConsts::StatBuffer";
            creation.m_Type = benzin::BufferType::Format;
            creation.m_DxgiFormat = statFormat;
            creation.m_ElementSizeInBytes = statElementSizeInBytes;
            creation.m_ElementCount = statElementCount;
            creation.m_IsUnorderedAccessAllowed = true;
        });

        m_ReadbackStatBuffer = ms_Device->GetPersistentReadbackAllocator().AllocateBuffer([&](benzin::BufferCreation& creation)
        {
            creation.m_DebugName = "GlobalConsts::ReadbackStatBuffer";
            creation.m_Type = benzin::BufferType::Format;
            creation.m_DxgiFormat = statFormat;
            creation.m_ElementSizeInBytes = statElementSizeInBytes;
            creation.m_ElementCount = statElementCount * BENZIN_READBACK_LATENCY;
        });
    }

    GlobalConstsPass::~GlobalConstsPass() = default;

    void GlobalConstsPass::OnUpdate()
    {
        BenzinProfile();

        const benzin::PerspectiveCamera& camera = ms_Scene->m_Camera;

        joint::CameraConsts cameraConsts = {};
        cameraConsts.m_WorldToView = camera.GetWorldToView();
        cameraConsts.m_ViewToWorld = camera.GetViewToWorld();
        cameraConsts.m_ViewToClip = camera.GetViewToClip();
        cameraConsts.m_ClipToView = camera.GetClipToView();
        cameraConsts.m_WorldToClip = camera.GetWorldToClip();
        cameraConsts.m_ClipToWorld = camera.GetClipToWorld();
        cameraConsts.m_ClipToWorldNoTranslation = camera.GetClipToWorldNoTranslation();
        cameraConsts.m_WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition());
        cameraConsts.m_UvToViewScale = camera.GetUvToViewScale();
        cameraConsts.m_UvToViewBias = camera.GetUvToViewBias();
        cameraConsts.m_PixelToWorldScale = camera.GetPixelToWorldScale(ms_RenderViewportHeight);
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Left] = camera.GetViewFrustumLeft();
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Right] = camera.GetViewFrustumRight();
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Bottom] = camera.GetViewFrustumBottom();
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Top] = camera.GetViewFrustumTop();
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Near] = camera.GetViewFrustumNear();
        cameraConsts.m_ViewFrustumPlanes[*joint::FrustumPlane::Far] = camera.GetViewFrustumFar();

        m_FrameConsts.m_PrevCamera = std::exchange(m_FrameConsts.m_Camera, cameraConsts);

        const DirectX::XMUINT2 renderResolution{ ms_RenderViewportWidth, ms_RenderViewportHeight };
        const float animationTimeInSec = ms_AnimationTimer->GetElapsedTimeInSec();

        m_FrameConsts.m_RenderResolution = { (float)renderResolution.x, (float)renderResolution.y };
        m_FrameConsts.m_InvRenderResolution = { 1.0f / (float)renderResolution.x, 1.0f / (float)renderResolution.y };
        m_FrameConsts.m_MinRenderDimension = (float)std::min(renderResolution.x, renderResolution.y);

        m_FrameConsts.m_CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex();

        m_FrameConsts.m_IsRenderResolutionChanged = renderResolution.x != m_PrevRenderResolution.x || renderResolution.y != m_PrevRenderResolution.y;
        m_FrameConsts.m_IsFrustumCullingEnabled = ms_Settings->GetSection<GBufferSettings>().m_IsFrustumCullingEnabled;
        m_FrameConsts.m_IsLodSelectionEnabled = ms_Settings->GetSection<GBufferSettings>().m_IsLodSelectionEnabled;
        m_FrameConsts.m_IsDenoiserEnabled = ms_Settings->GetSection<SigmaDenoiserSettings>().m_IsEnabled;

        m_FrameConsts.m_DeltaTimeInSec = ms_FrameTimer->GetDeltaTimeInSec();
        m_FrameConsts.m_AnimationElapsedTimeInSec = animationTimeInSec;
        m_FrameConsts.m_PrevAnimationElapsedTimeInSec = m_PrevAnimationElapsedTimeInSec;

        m_PrevRenderResolution = renderResolution;
        m_PrevAnimationElapsedTimeInSec = animationTimeInSec;

        m_FrameConsts.m_SunLight.m_Color = ms_Scene->m_SunLight.m_Color;
        m_FrameConsts.m_SunLight.m_Intensity = ms_Scene->m_SunLight.m_Intensity;
        m_FrameConsts.m_SunLight.m_Direction = CalcToSunDirection(ms_Scene->m_SunLight);
        m_FrameConsts.m_SunLight.m_TanOfAngularRadius = std::tan(ms_Scene->m_SunLight.m_AngularDiameterInRadians * 0.5f);
    }

    void GlobalConstsPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("GlobalConstsPass");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        {
            BenzinProfile();
            BenzinGpuProfile("CopyStats");

            const uint64_t dataSizeInBytes = m_StatBuffer->GetSizeInBytes();
            cmdList.CopyBufferRegion(
                *m_ReadbackStatBuffer,
                dataSizeInBytes * ms_Device->GetReadbackWriteIndex(),
                *m_StatBuffer,
                0,
                dataSizeInBytes);

            m_ReadbackStatBuffer->MapReadbackData<uint32_t>(
                (uint32_t)m_StatBuffer->GetElementCount() * ms_Device->GetReadbackReadIndex(),
                (uint32_t)m_StatBuffer->GetElementCount(),
                [this](std::span<const uint32_t> readbackStats)
                {
                    m_ReadbackStatsCallback(readbackStats);
                });
        }

        {
            BenzinGpuEvent("SetUnifiedRootParameters");

            const uint64_t frameConstsGpuAddress = ms_Device->GetConstBufferAllocator().Allocate(m_FrameConsts);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConsts, frameConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConsts, frameConstsGpuAddress);

            const uint64_t statBufferGpuAddress = m_StatBuffer->GetGpuVirtualAddress();
            cmdList.AddTransition(*m_StatBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            cmdList.FlushBarriers();
            cmdList.ClearUnorderedAccess(*m_StatBuffer, m_StatBuffer->GetUav());
            cmdList.SetComputeUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
            cmdList.SetGraphicsUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
        }
    }

}
