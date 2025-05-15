#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/global_consts_pass.hpp"

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/core/tick_timer.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <sandbox/sandbox_render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::FrustumPlane);

namespace sandbox
{

    GlobalConstsPass::GlobalConstsPass(ReadbackStatsCallback&& callback)
        : m_ReadbackStatsCallback{ std::move(callback) }
    {
        BenzinAssert(m_ReadbackStatsCallback);

        const auto statFormat = benzin::GraphicsFormat::R32Uint;
        const uint32_t statElementSizeInBytes = benzin::GetFormatSizeInBytes(statFormat);
        const uint32_t statElementCount = (uint32_t)magic_enum::enum_count<joint::ReadbackStat>();

        // Usage as ByteAddressBuffer
        ms_Resources->Create(BufferId::UavStats, benzin::BufferCreation
        {
            .DebugName = magic_enum::enum_name(BufferId::UavStats),
            .Type = benzin::BufferType::Format,
            .Format = benzin::GraphicsFormat::R32Uint,
            .ElementSizeInBytes = statElementSizeInBytes,
            .ElementCount = statElementCount,
            .IsUnorderedAccessAllowed = true,
        });

        ms_Resources->Create(BufferId::ReadbackStats, benzin::BufferCreation
        {
            .DebugName = magic_enum::enum_name(BufferId::ReadbackStats),
            .MemoryType = benzin::ResourceMemoryType::Readback,
            .Type = benzin::BufferType::Format,
            .Format = statFormat,
            .ElementSizeInBytes = statElementSizeInBytes,
            .ElementCount = statElementCount * benzin::CmdLineArgs::GetReadbackLatency(),
        });

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ ms_Resources->Get(BufferId::UavStats), benzin::ResourceState::UnorderedAccess });
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ ms_Resources->Get(BufferId::ReadbackStats), benzin::ResourceState::Common });
    }

    GlobalConstsPass::~GlobalConstsPass()
    {
        ms_Resources->Destroy(BufferId::UavStats);
        ms_Resources->Destroy(BufferId::ReadbackStats);
    }

    void GlobalConstsPass::OnUpdate()
    {
        BenzinProfile();

        UpdateCameraConsts();
        UpdateFrameConsts();

        ms_ConstBufferPool->PreAllocate(sizeof(m_FrameConsts));
    }

    void GlobalConstsPass::OnRender() const
    {
        BenzinProfile();

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "GlobalConstsPass");

        CopyStats(cmdList);

        {
            BenzinGpuEvent(cmdList, "SetUnifiedRootParameters");

            const uint64_t frameConstsGpuAddress = ms_ConstBufferPool->Allocate(m_FrameConsts);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);

            const uint64_t lightBufferGpuAddress = ms_Scene->GetLightBufferGpuAddress();
            cmdList.SetComputeSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, lightBufferGpuAddress);
            cmdList.SetGraphicsSrv(benzin::UnifiedRootParameter::LightStructuredBuffer, lightBufferGpuAddress);

            const benzin::Buffer& statsBuffer = ms_Resources->Get(BufferId::UavStats);
            cmdList.ClearUnorderedAccess(statsBuffer, statsBuffer.GetUav(), {});
            cmdList.SetComputeUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statsBuffer.GetGpuVirtualAddress());
            cmdList.SetGraphicsUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statsBuffer.GetGpuVirtualAddress());
        }
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

    void GlobalConstsPass::CopyStats(benzin::GraphicsCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "CopyStats");

        const benzin::Buffer& destBuffer = ms_Resources->Get(BufferId::ReadbackStats);
        const benzin::Buffer& sourceBuffer = ms_Resources->Get(BufferId::UavStats);

        const uint32_t dataSizeInBytes = (uint32_t)sourceBuffer.GetSizeInBytes();
        const uint64_t destOffsetInBytes = (ms_Device->GetCpuFrameIndex() % benzin::CmdLineArgs::GetReadbackLatency()) * dataSizeInBytes;
        const uint64_t readbackOffsetInBytes = ((ms_Device->GetCpuFrameIndex() + 1) % benzin::CmdLineArgs::GetReadbackLatency()) * dataSizeInBytes;

        cmdList.CopyBufferRegion(destBuffer, destOffsetInBytes, sourceBuffer, 0, dataSizeInBytes);

        destBuffer.MapReadbackData(readbackOffsetInBytes, dataSizeInBytes, [this](const std::byte* mappedData)
        {
            const auto readbackStats = benzin::ToSpan((const uint32_t*)mappedData, magic_enum::enum_count<joint::ReadbackStat>());

            m_ReadbackStatsCallback(readbackStats);
        });
    }

}
