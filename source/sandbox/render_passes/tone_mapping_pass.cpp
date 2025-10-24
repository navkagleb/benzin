#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/tone_mapping_pass.hpp>

#include <sandbox/render_settings.hpp>
#include <sandbox/resources.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

BenzinAllowDereferenceOperatorForEnum(joint::CalcLuminanceHistogramResources);
BenzinAllowDereferenceOperatorForEnum(joint::CalcAvgLuminanceResources);
BenzinAllowDereferenceOperatorForEnum(joint::ApplyToneMapOperatorResources);

namespace sandbox
{

    ToneMappingPass::ToneMappingPass()
    {
        const auto createPso = [](PsoId id, std::string_view csFileName)
        {
            ms_PsoManager->Create(id, [id, csFileName](benzin::ComputePsoProxy& proxy)
            {
                proxy.Cs.FileName = csFileName;
            });
        };

        createPso(PsoId::ToneMapping_CalcLuminanceHistogram, "calc_luminance_histogram.hlsl");
        createPso(PsoId::ToneMapping_CalcAvgLuminance, "calc_avg_luminance.hlsl");
        createPso(PsoId::ToneMapping_ApplyToneMapOperator, "apply_tone_map_operator.hlsl");

        const uint32_t luminanceHistogramWidth = 16;
        const uint32_t luminanceHistogramHeight = 16;

        ms_Resources->Create(BufferId::ToneMapping_LuminanceHistogram, benzin::BufferCreation
        {
            .DebugName = "ToneMapping_LuminanceHistogram",
            .HeapType = benzin::GpuHeapType::Default,
            .Type = benzin::BufferType::Format,
            .Format = benzin::GraphicsFormat::R32Uint,
            .ElementSizeInBytes = sizeof(uint32_t),
            .ElementCount = luminanceHistogramWidth * luminanceHistogramHeight,
            .IsUnorderedAccessAllowed = true,
        });

        ms_Resources->Create(TextureId::ToneMapping_AvgLuminance, benzin::TextureCreation
        {
            .DebugName = "ToneMapping_AvgLuminance",
            .Format = benzin::GraphicsFormat::R16Float,
            .Width = 1,
            .Height = 1,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        ms_Resources->Create(TextureId::ToneMapping_DebugLuminanceHistogram, benzin::TextureCreation
        {
            .DebugName = "ToneMapping_DebugLuminanceHistogram",
            .Format = benzin::GraphicsFormat::R8Unorm,
            .Width = luminanceHistogramWidth,
            .Height = luminanceHistogramHeight,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });
    }

    ToneMappingPass::~ToneMappingPass()
    {
        ms_PsoManager->Destroy(PsoId::ToneMapping_CalcLuminanceHistogram);
        ms_PsoManager->Destroy(PsoId::ToneMapping_CalcAvgLuminance);
        ms_PsoManager->Destroy(PsoId::ToneMapping_ApplyToneMapOperator);

        ms_Resources->Destroy(BufferId::ToneMapping_LuminanceHistogram);
        ms_Resources->Destroy(TextureId::ToneMapping_AvgLuminance);
        ms_Resources->Destroy(TextureId::ToneMapping_DebugLuminanceHistogram);
        ms_Resources->Destroy(TextureId::Final);
    }

    void ToneMappingPass::OnRenderViewportResize()
    {
        ms_Resources->Create(TextureId::Final, benzin::TextureCreation
        {
            .DebugName = "Final",
            .Format = benzin::GraphicsFormat::Rgba8Unorm,
            .Width = ms_RenderViewportWidth,
            .Height = ms_RenderViewportHeight,
            .MipCount = 1,
            .AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess | benzin::TextureAccessFlag::AllowRenderTarget,
        });
    }

    void ToneMappingPass::OnUpdate()
    {
        const auto& settings = ms_Settings->GetSection<ToneMappingSettings>();

        {
            const auto& luminanceHistogram = settings.LuminanceHistogram;

            m_Consts.LuminanceHistogram.MinLogLuminance = luminanceHistogram.MinLogLuminance;
            m_Consts.LuminanceHistogram.LogLuminanceRange = luminanceHistogram.MaxLogLuminance - luminanceHistogram.MinLogLuminance;
            m_Consts.LuminanceHistogram.InvLogLuminanceRange = 1.0f / m_Consts.LuminanceHistogram.LogLuminanceRange;
            m_Consts.LuminanceHistogram.TimeFactor = luminanceHistogram.Tau;
        }

        m_Consts.PbrCamera = settings.PbrCamera;

        m_Consts.ToneReproductionTransform = settings.ToneReproductionTransform;

        m_Consts.IsToneMappingEnabled = settings.IsToneMappingEnabled;
        m_Consts.IsAutoExposureUsed = settings.IsAutoExposureUsed;
        m_Consts.IsAccurateGammaCorrectionUsed = settings.IsAccurateGammaCorrectionUsed;
    }

    void ToneMappingPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("ToneMapping");

        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstBuffer0, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

        RunClearPass(cmdList);
        RunCalcLuminanceHistogramPass(cmdList);
        RunCalcAvgLuminancePass(cmdList);
        RunApplyToneMapOperatorPass(cmdList);
    }

    void ToneMappingPass::RunClearPass(benzin::ComputeCmdList& cmdList) const
    {
        static bool isFirstTime = true;

        if (!isFirstTime)
            return;

        BenzinGpuEvent("ClearPass");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& avgLuminance = ms_Resources->Get(TextureId::ToneMapping_AvgLuminance);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
            benzin::TransitionBarrier{ avgLuminance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS });

        cmdList.ClearUnorderedAccess(luminanceHistogram, luminanceHistogram.GetUav(), {});
        cmdList.ClearUnorderedAccess(avgLuminance, avgLuminance.GetUav(), {});

        isFirstTime = false;
    }

    void ToneMappingPass::RunCalcLuminanceHistogramPass(benzin::ComputeCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("CalcLuminanceHistogram");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& debugLuminanceHistogram = ms_Resources->Get(TextureId::ToneMapping_DebugLuminanceHistogram);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
            benzin::TransitionBarrier{ debugLuminanceHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS });

        {
            using Resources = joint::CalcLuminanceHistogramResources;

            cmdList.SetComputeRootResource(*Resources::HdrColor, ms_Resources->Get(TextureId::HdrColor).GetSrv());
            cmdList.SetComputeRootResource(*Resources::OutLuminanceHistogram, luminanceHistogram.GetUav());
            cmdList.SetComputeRootResource(*Resources::OutDebugLuminanceHistogram, debugLuminanceHistogram.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcLuminanceHistogram));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });
    }

    void ToneMappingPass::RunCalcAvgLuminancePass(benzin::ComputeCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("CalcAvgLuminance");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& avgLuminance = ms_Resources->Get(TextureId::ToneMapping_AvgLuminance);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS },
            benzin::TransitionBarrier{ avgLuminance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS });

        {
            using Resources = joint::CalcAvgLuminanceResources;

            cmdList.SetComputeRootResource(*Resources::OutLuminanceHistogram, luminanceHistogram.GetUav());
            cmdList.SetComputeRootResource(*Resources::OutAvgLuminance, avgLuminance.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcAvgLuminance));
        cmdList.Dispatch({ 1, 1, 1 }, { 1, 1, 1 });
    }

    void ToneMappingPass::RunApplyToneMapOperatorPass(benzin::ComputeCmdList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile("ApplyToneMapOperator");

        const auto& finalTexture = ms_Resources->Get(TextureId::Final);

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ finalTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS });

        {
            using Resources = joint::ApplyToneMapOperatorResources;

            cmdList.SetComputeRootResource(*Resources::AvgLuminance, ms_Resources->Get(TextureId::ToneMapping_AvgLuminance).GetSrv());
            cmdList.SetComputeRootResource(*Resources::HdrColor, ms_Resources->Get(TextureId::HdrColor).GetSrv());
            cmdList.SetComputeRootResource(*Resources::OutFinal, finalTexture.GetUav());
        }

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_ApplyToneMapOperator));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });
    }

}
