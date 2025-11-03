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
        const auto createPso = [](PsoId id, std::string_view fileName)
        {
            ms_PsoManager->Create(id, [id, fileName](benzin::ComputePsoProxy& proxy)
            {
                proxy.m_Cs.m_FileName = fileName;
            });
        };

        createPso(PsoId::ToneMapping_CalcLuminanceHistogram, "calc_luminance_histogram.hlsl");
        createPso(PsoId::ToneMapping_CalcAvgLuminance, "calc_avg_luminance.hlsl");
        createPso(PsoId::ToneMapping_ApplyToneMapOperator, "apply_tone_map_operator.hlsl");

        const uint32_t luminanceHistogramWidth = 16;
        const uint32_t luminanceHistogramHeight = 16;

        ms_Resources->Create(BufferId::ToneMapping_LuminanceHistogram, benzin::BufferCreation
        {
            .m_DebugName = "ToneMapping_LuminanceHistogram",
            .m_HeapType = benzin::GpuHeapType::Default,
            .m_Type = benzin::BufferType::Format,
            .m_DxgiFormat = DXGI_FORMAT_R32_UINT,
            .m_ElementSizeInBytes = sizeof(uint32_t),
            .m_ElementCount = luminanceHistogramWidth * luminanceHistogramHeight,
            .m_IsUnorderedAccessAllowed = true,
        });

        ms_Resources->Create(TextureId::ToneMapping_AvgLuminance, benzin::TextureCreation
        {
            .m_DebugName = "ToneMapping_AvgLuminance",
            .m_DxgiFormat = DXGI_FORMAT_R16_FLOAT,
            .m_Width = 1,
            .m_Height = 1,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
        });

        ms_Resources->Create(TextureId::ToneMapping_DebugLuminanceHistogram, benzin::TextureCreation
        {
            .m_DebugName = "ToneMapping_DebugLuminanceHistogram",
            .m_DxgiFormat = DXGI_FORMAT_R8_UNORM,
            .m_Width = luminanceHistogramWidth,
            .m_Height = luminanceHistogramHeight,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess,
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
            .m_DebugName = "Final",
            .m_DxgiFormat = DXGI_FORMAT_R8G8B8A8_UNORM,
            .m_Width = ms_RenderViewportWidth,
            .m_Height = ms_RenderViewportHeight,
            .m_MipCount = 1,
            .m_AccessFlags = benzin::TextureAccessFlag::AllowUnorderedAccess | benzin::TextureAccessFlag::AllowRenderTarget,
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

        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConsts, ms_Device->GetConstBufferAllocator().Allocate(m_Consts));

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

        cmdList.AddTransition(luminanceHistogram, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.AddTransition(avgLuminance, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        cmdList.FlushBarriers();

        cmdList.ClearUnorderedAccess(luminanceHistogram, luminanceHistogram.GetUav());
        cmdList.ClearUnorderedAccess(avgLuminance, avgLuminance.GetUav());

        cmdList.AddUnorderedAccess(luminanceHistogram);
        cmdList.AddUnorderedAccess(avgLuminance);

        isFirstTime = false;
    }

    void ToneMappingPass::RunCalcLuminanceHistogramPass(benzin::ComputeCmdList& cmdList) const
    {
        using Resources = joint::CalcLuminanceHistogramResources;

        BenzinProfile();
        BenzinGpuProfile("CalcLuminanceHistogram");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& debugLuminanceHistogram = ms_Resources->Get(TextureId::ToneMapping_DebugLuminanceHistogram);

        cmdList.SetComputeRootSrv(*Resources::HdrColor, ms_Resources->Get(TextureId::HdrColor));
        cmdList.SetComputeRootUav(*Resources::OutLuminanceHistogram, luminanceHistogram);
        cmdList.SetComputeRootUav(*Resources::OutDebugLuminanceHistogram, debugLuminanceHistogram);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcLuminanceHistogram));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });

        cmdList.AddUnorderedAccess(luminanceHistogram);
        cmdList.AddUnorderedAccess(debugLuminanceHistogram);
    }

    void ToneMappingPass::RunCalcAvgLuminancePass(benzin::ComputeCmdList& cmdList) const
    {
        using Resources = joint::CalcAvgLuminanceResources;

        BenzinProfile();
        BenzinGpuProfile("CalcAvgLuminance");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& avgLuminance = ms_Resources->Get(TextureId::ToneMapping_AvgLuminance);

        cmdList.SetComputeRootUav(*Resources::OutLuminanceHistogram, luminanceHistogram);
        cmdList.SetComputeRootUav(*Resources::OutAvgLuminance, avgLuminance);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcAvgLuminance));
        cmdList.Dispatch({ 1, 1, 1 }, { 1, 1, 1 });

        cmdList.AddUnorderedAccess(luminanceHistogram);
        cmdList.AddUnorderedAccess(avgLuminance);
    }

    void ToneMappingPass::RunApplyToneMapOperatorPass(benzin::ComputeCmdList& cmdList) const
    {
        using Resources = joint::ApplyToneMapOperatorResources;

        BenzinProfile();
        BenzinGpuProfile("ApplyToneMapOperator");

        const auto& finalTexture = ms_Resources->Get(TextureId::Final);

        cmdList.SetComputeRootSrv(*Resources::HdrColor, ms_Resources->Get(TextureId::HdrColor));
        cmdList.SetComputeRootSrv(*Resources::AvgLuminance, ms_Resources->Get(TextureId::ToneMapping_AvgLuminance));
        cmdList.SetComputeRootUav(*Resources::OutFinal, finalTexture);
        cmdList.FlushBarriers();

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_ApplyToneMapOperator));
        cmdList.Dispatch({ ms_RenderViewportWidth, ms_RenderViewportHeight, 1 }, { 16, 16, 1 });

        cmdList.AddUnorderedAccess(finalTexture);
    }

}
