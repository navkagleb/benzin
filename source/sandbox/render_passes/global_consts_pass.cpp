#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/global_consts_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

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

        constexpr DXGI_FORMAT statFormat = DXGI_FORMAT_R32_UINT;
        const uint32_t statElementSizeInBytes = benzin::GetDxgiFormatSizeInBytes(statFormat);
        const uint64_t statElementCount = magic_enum::enum_count<joint::ReadbackStat>();

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
            m_FrameConsts.IsDenoiserEnabled = ms_Settings->GetSection<SigmaDenoiserSettings>().m_IsEnabled;

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
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::FrameConsts, frameConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::FrameConsts, frameConstsGpuAddress);

            // TODO: replace with joint::SunLight struct
            joint::Light sunLight = {};
            sunLight.Color = ms_Scene->m_SunLight.m_Color;
            sunLight.Intensity = ms_Scene->m_SunLight.m_Intensity;
            sunLight.WorldPosition = ms_Scene->m_SunLight.CalcToSunDirection();
            sunLight.WorldRadius = std::tan(ms_Scene->m_SunLight.m_AngularDiameterInRadians * 0.5f);
            sunLight.Attenuation = {};
            sunLight.Type = joint::LightType::Sun;

            const uint64_t sunLightConstsGpuAddress = ms_Device->GetConstBufferAllocator().Allocate(sunLight);
            cmdList.SetComputeCbv(benzin::UnifiedRootParameter::SunLightConsts, sunLightConstsGpuAddress);
            cmdList.SetGraphicsCbv(benzin::UnifiedRootParameter::SunLightConsts, sunLightConstsGpuAddress);

            const uint64_t statBufferGpuAddress = m_StatBuffer->GetGpuVirtualAddress();
            cmdList.AddTransition(*m_StatBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
            cmdList.ClearUnorderedAccess(*m_StatBuffer, m_StatBuffer->GetUav());
            cmdList.SetComputeUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
            cmdList.SetGraphicsUav(benzin::UnifiedRootParameter::ReadbackStatsBuffer, statBufferGpuAddress);
        }
    }

}
