#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/cmd_line_args.hpp>
#include <benzin/core/profiler.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

#include <shaders/joint/light.hpp>

namespace sandbox
{

    GlobalConstsPass::GlobalConstsPass(ReadbackStatsCallback&& callback)
        : m_ReadbackStatsCallback{ std::move(callback) }
    {
        BenzinAssert(m_ReadbackStatsCallback);

        const auto statFormat = benzin::GraphicsFormat::R32Uint;
        const uint32_t statElementSizeInBytes = benzin::GetFormatSizeInBytes(statFormat);
        const uint64_t statElementCount = magic_enum::enum_count<joint::ReadbackStat>();

        // Usage as ByteAddressBuffer
        m_StatBuffer = ms_Device->GetPersistentDefaultLinearAllocator().AllocateBuffer([&](benzin::BufferCreation& creation)
        {
            creation.DebugName = "GlobalConsts::StatBuffer";
            creation.Type = benzin::BufferType::Format;
            creation.Format = statFormat;
            creation.ElementSizeInBytes = statElementSizeInBytes;
            creation.ElementCount = statElementCount;
            creation.IsUnorderedAccessAllowed = true;
        });

        m_ReadbackStatBuffer = ms_Device->GetPersistentReadbackLinearAllocator().AllocateBuffer([&](benzin::BufferCreation& creation)
        {
            creation.DebugName = "GlobalConsts::ReadbackStatBuffer";
            creation.Type = benzin::BufferType::Format;
            creation.Format = statFormat;
            creation.ElementSizeInBytes = statElementSizeInBytes;
            creation.ElementCount = statElementCount * BENZIN_READBACK_LATENCY;
        });

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *m_StatBuffer, benzin::ResourceState::UnorderedAccess });
        cmdList.AddResourceBarrier(benzin::TransitionBarrier{ *m_ReadbackStatBuffer, benzin::ResourceState::Common });
    }

    GlobalConstsPass::~GlobalConstsPass() = default;

    void GlobalConstsPass::OnUpdate()
    {
        BenzinProfile();

        {
            const benzin::PerspectiveCamera& camera = ms_Scene->m_Camera;

            joint::CameraConsts cameraConsts = {};
            cameraConsts.WorldToView = camera.GetWorldToViewMatrix();
            cameraConsts.ViewToWorld = camera.GetViewToWorldMatrix();
            cameraConsts.ViewToClip = camera.GetViewToClipMatrix();
            cameraConsts.ClipToView = camera.GetClipToViewMatrix();
            cameraConsts.WorldToClip = camera.GetWorldToClipMatrix();
            cameraConsts.ClipToWorld = camera.GetClipToWorldMatrix();
            cameraConsts.ClipToWorldNoTranslation = camera.GetClipToWorldNoTranslation();
            cameraConsts.WorldPosition = *reinterpret_cast<const DirectX::XMFLOAT3*>(&camera.GetPosition());
            cameraConsts.TanHalfFovX = camera.GetTanHalfFovX();
            cameraConsts.TanHalfFovY = camera.GetTanHalfFovY();
            cameraConsts.NearPlane = camera.GetNearPlane();
            cameraConsts.FarPlane = camera.GetFarPlane();
            cameraConsts.UvToViewScale = camera.GetUvToViewScale();
            cameraConsts.UvToViewBias = camera.GetUvToViewBias();
            cameraConsts.PixelToWorldScale = camera.GetPixelToWorldScale(ms_RenderViewportHeight);

            if (ms_Device->GetCpuFrameIndex() != 0) // TODO: Remove if
            {
                m_FrameConsts.PrevCamera = std::exchange(m_FrameConsts.Camera, cameraConsts);
            }
            else
            {
                m_FrameConsts.Camera = cameraConsts;
                m_FrameConsts.PrevCamera = cameraConsts;
            }
        }

        {
            const DirectX::XMUINT2 renderResolution{ ms_RenderViewportWidth, ms_RenderViewportHeight };
            const float animationTimeInSec = ms_AnimationTimer->GetElapsedTimeInSec();

            m_FrameConsts.RenderResolution = { (float)renderResolution.x, (float)renderResolution.y };
            m_FrameConsts.InvRenderResolution = { 1.0f / (float)renderResolution.x, 1.0f / (float)renderResolution.y };
            m_FrameConsts.MinRenderDimension = (float)std::min(renderResolution.x, renderResolution.y);

            m_FrameConsts.CpuFrameIndex = (uint32_t)ms_Device->GetCpuFrameIndex();

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

    void GlobalConstsPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("GlobalConstsPass");

        benzin::GraphicsCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        {
            BenzinProfile();
            BenzinGpuProfile("CopyStats");

            const uint64_t dataSizeInBytes = m_StatBuffer->GetSizeInBytes();
            const uint64_t destOffsetInBytes = (ms_Device->GetCpuFrameIndex() % BENZIN_READBACK_LATENCY) * dataSizeInBytes;
            const uint64_t readbackOffsetInBytes = ((ms_Device->GetCpuFrameIndex() + 1) % BENZIN_READBACK_LATENCY) * dataSizeInBytes;

            cmdList.CopyBufferRegion(*m_ReadbackStatBuffer, destOffsetInBytes, *m_StatBuffer, 0, dataSizeInBytes);

            m_ReadbackStatBuffer->MapReadbackData(readbackOffsetInBytes, dataSizeInBytes, [this](const std::byte* mappedData)
            {
                const auto readbackStats = benzin::ToSpan((const uint32_t*)mappedData, magic_enum::enum_count<joint::ReadbackStat>());
                m_ReadbackStatsCallback(readbackStats);
            });
        }

        {
            BenzinGpuEvent("SetUnifiedRootParameters");

            const uint64_t frameConstsGpuAddress = ms_Device->GetConstBufferAllocator().Allocate(m_FrameConsts);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConstBuffer, frameConstsGpuAddress);

            // TODO: replace with joint::SunLight struct
            joint::Light sunLight = {};
            sunLight.Color = ms_Scene->m_SunLight.GetColor();
            sunLight.Intensity = ms_Scene->m_SunLight.GetIntensity();
            sunLight.WorldPosition = ms_Scene->m_SunLight.CalcToSunDirection();
            sunLight.WorldRadius = std::tan(ms_Scene->m_SunLight.GetAngularDiameterInRadians() * 0.5f);
            sunLight.Attenuation = {};
            sunLight.Type = joint::LightType::Sun;

            const uint64_t sunLightConstsGpuAddress = ms_Device->GetConstBufferAllocator().Allocate(sunLight);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::SunLightConstBuffer, sunLightConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::SunLightConstBuffer, sunLightConstsGpuAddress);

            const uint64_t statBufferGpuAddress = m_StatBuffer->GetGpuVirtualAddress();
            cmdList.ClearUnorderedAccess(*m_StatBuffer, m_StatBuffer->GetUav(), {});
            cmdList.SetComputeUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
            cmdList.SetGraphicsUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
        }
    }

}
