#include <sandbox/bootstrap.hpp>
#include <sandbox/render_passes/tone_mapping_pass.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/texture.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/const_buffer_pool.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <sandbox/sandbox_render_settings.hpp>
#include <sandbox/resources.hpp>

BenzinEnableUnaryPlusForEnum(joint::CalcLuminanceHistogramResources);
BenzinEnableUnaryPlusForEnum(joint::CalcAvgLuminanceResources);
BenzinEnableUnaryPlusForEnum(joint::ApplyToneMapOperatorResources);

namespace sandbox
{

    ToneMappingPass::ToneMappingPass()
    {
        const auto createPso = [](PsoId id, std::string_view csFileName)
        {
            ms_PsoManager->Create(id, [id, csFileName](benzin::ComputePsoProxy& proxy)
            {
                proxy.DebugName = magic_enum::enum_name(id);
                proxy.CsFileName = csFileName;
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
            .Type = benzin::BufferType::Format,
            .Format = benzin::GraphicsFormat::R32Uint,
            .ElementSize = sizeof(uint32_t),
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

        ms_ConstBufferPool->PreAllocate(sizeof(m_Consts));
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
            .Width = GetRenderViewportWidth(),
            .Height = GetRenderViewportHeight(),
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

        auto& cmdList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ToneMapping");

        cmdList.SetComputeCbv(benzin::UnifiedRootParameter::RenderPassConstantBuffer0, ms_ConstBufferPool->Allocate(m_Consts));

        RunClearPass(cmdList);
        RunCalcLuminanceHistogramPass(cmdList);
        RunCalcAvgLuminancePass(cmdList);
        RunApplyToneMapOperatorPass(cmdList);
    }

    void ToneMappingPass::RunClearPass(benzin::GraphicsCommandList& cmdList) const
    {
        static bool isFirstTime = true;

        if (!isFirstTime)
        {
            return;
        }

        BenzinGpuEvent(cmdList, "ClearPass");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& avgLuminance = ms_Resources->Get(TextureId::ToneMapping_AvgLuminance);

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ avgLuminance, benzin::ResourceState::UnorderedAccess },
        );

        cmdList.ClearUnorderedAccess(luminanceHistogram, luminanceHistogram.GetUav(), {});
        cmdList.ClearUnorderedAccess(avgLuminance, avgLuminance.GetUav(), {});

        isFirstTime = false;
    }

    void ToneMappingPass::RunCalcLuminanceHistogramPass(benzin::GraphicsCommandList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "CalcLuminanceHistogram");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& debugLuminanceHistogram = ms_Resources->Get(TextureId::ToneMapping_DebugLuminanceHistogram);

        {
            using enum joint::CalcLuminanceHistogramResources;

            cmdList.SetComputeRootResource(+HdrColor, ms_Resources->Get(TextureId::HdrColor).GetSrv());
            cmdList.SetComputeRootResource(+OutLuminanceHistogram, luminanceHistogram.GetUav());
            cmdList.SetComputeRootResource(+OutDebugLuminanceHistogram, debugLuminanceHistogram.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ debugLuminanceHistogram, benzin::ResourceState::UnorderedAccess },
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcLuminanceHistogram));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

    void ToneMappingPass::RunCalcAvgLuminancePass(benzin::GraphicsCommandList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "CalcAvgLuminance");

        const auto& luminanceHistogram = ms_Resources->Get(BufferId::ToneMapping_LuminanceHistogram);
        const auto& avgLuminance = ms_Resources->Get(TextureId::ToneMapping_AvgLuminance);

        {
            using enum joint::CalcAvgLuminanceResources;

            cmdList.SetComputeRootResource(+OutLuminanceHistogram, luminanceHistogram.GetUav());
            cmdList.SetComputeRootResource(+OutAvgLuminance, avgLuminance.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ luminanceHistogram, benzin::ResourceState::UnorderedAccess },
            benzin::TransitionBarrier{ avgLuminance, benzin::ResourceState::UnorderedAccess },
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_CalcAvgLuminance));
        cmdList.Dispatch({ 1, 1, 1 }, { 1, 1, 1 });
    }

    void ToneMappingPass::RunApplyToneMapOperatorPass(benzin::GraphicsCommandList& cmdList) const
    {
        BenzinProfile();
        BenzinGpuProfile(*ms_GpuProfiler, cmdList, "ApplyToneMapOperator");

        const auto& finalTexture = ms_Resources->Get(TextureId::Final);

        {
            using enum joint::ApplyToneMapOperatorResources;

            cmdList.SetComputeRootResource(+AvgLuminance, ms_Resources->Get(TextureId::ToneMapping_AvgLuminance).GetSrv());
            cmdList.SetComputeRootResource(+HdrColor, ms_Resources->Get(TextureId::HdrColor).GetSrv());
            cmdList.SetComputeRootResource(+OutFinal, finalTexture.GetUav());
        }

        BenzinMakeScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ finalTexture, benzin::ResourceState::UnorderedAccess },
        );

        cmdList.SetComputePso(ms_PsoManager->GetCompute(PsoId::ToneMapping_ApplyToneMapOperator));
        cmdList.Dispatch({ GetRenderViewportWidth(), GetRenderViewportHeight(), 1 }, { 16, 16, 1 });
    }

}
