#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/pso_manager.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"
#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/pso.hpp"
#include "benzin/graphics/ray_tracing_pso.hpp"
#include "benzin/graphics2/shader_manager.hpp"

namespace benzin
{

    PsoManager::PsoManager(Device& device, ShaderManager& shaderManager, uint32_t psoCount)
        : m_Device{ device }
        , m_ShaderManager{ shaderManager }
    {
        m_Psos.resize(psoCount);

        m_ShaderManager.SetNewShaderAvailableCallback([this] { RecompilePsoCallback(); });
    }

    PsoManager::~PsoManager()
    {
#if BENZIN_IS_DEBUG_BUILD
        uint32_t nonReleasedPsoCount = 0;
        for (const auto& pso : m_Psos)
        {
            nonReleasedPsoCount += (pso.get() != nullptr);
        }

        BenzinWarningIf(
            nonReleasedPsoCount != 0,
            "Not all PSOs are released properly. Non released PSO count: {}",
            nonReleasedPsoCount
        );
#endif

        m_Psos.clear();
    }

    void PsoManager::CreateGraphicsPso(uint32_t index, const GraphicsPsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        GraphicsPsoProxy proxy;
        configurator(proxy);

        auto& pso = m_Psos[index];
        BenzinAssert(pso.get() == nullptr);

        ShaderInfo vs{ ShaderType::Vertex, proxy.VsFileName, proxy.VsEntryPoint, std::move(proxy.VsDefines) };
        ShaderInfo ps{ ShaderType::Pixel, proxy.PsFileName, proxy.PsEntryPoint, std::move(proxy.PsDefines) };

        const auto vsBytecode = m_ShaderManager.GetShaderBytecode(vs);
        const auto psBytecode = m_ShaderManager.GetShaderBytecode(ps);

        auto graphicsPso = std::make_unique<GraphicsPso>(m_Device);
        graphicsPso->SetVs(std::move(vs), vsBytecode);
        graphicsPso->SetPs(std::move(ps), psBytecode);
        graphicsPso->SetPrimitiveTopologyType(proxy.PrimitiveTopologyType);
        graphicsPso->SetRasterizerState(proxy.RasterizerState);
        graphicsPso->SetDepthStencilState(proxy.DepthState, proxy.StencilState);
        graphicsPso->SetBlendState(proxy.BlendState);
        graphicsPso->SetRenderTargetFormats(proxy.RenderTargetFormats);
        graphicsPso->SetDepthStencilFormat(proxy.DepthStencilFormat);
        graphicsPso->Compile();

        SetDxObjectDebugName(graphicsPso->GetD3D12PipelineState(), proxy.DebugName);

        pso = std::move(graphicsPso);
    }

    void PsoManager::CreateComputePso(uint32_t index, const ComputePsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        ComputePsoProxy proxy;
        configurator(proxy);

        auto& pso = m_Psos[index];
        BenzinAssert(pso.get() == nullptr);

        ShaderInfo cs{ ShaderType::Compute, proxy.CsFileName, proxy.CsEntryPoint, std::move(proxy.CsDefines) };
        const auto csBytecode = m_ShaderManager.GetShaderBytecode(cs);

        auto computePso = std::make_unique<ComputePso>(m_Device);
        computePso->SetCs(std::move(cs), csBytecode);
        computePso->Compile();

        SetDxObjectDebugName(computePso->GetD3D12PipelineState(), proxy.DebugName);

        pso = std::move(computePso);
    }

    void PsoManager::CreateRayTracingPso(uint32_t index, const RayTracingPsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        RayTracing_PsoProxy proxy;
        configurator(proxy);

        auto& pso = m_Psos[index];
        BenzinAssert(pso.get() == nullptr);

        ShaderInfo library{ ShaderType::Library, proxy.ShaderLibrary.FileName, {}, std::move(proxy.ShaderLibrary.Defines)};
        const auto libraryBytecode = m_ShaderManager.GetShaderBytecode(library);

        auto rayTracingPso = std::make_unique<RayTracing_Pso>(m_Device);
        rayTracingPso->SetShaderLibrary(std::move(library), libraryBytecode);
        rayTracingPso->SetRayGenerationShader(proxy.RayGenerationEntryPoint);
        rayTracingPso->SetMissShader(proxy.MissEntryPoint);
        rayTracingPso->SetHitGroup(proxy.HitGroup.Name, proxy.HitGroup.ClosestHitEntryPoint);
        rayTracingPso->SetShaderConfig(proxy.ShaderConfig.PayloadSize, proxy.ShaderConfig.AttributeSize);
        rayTracingPso->Compile();

        SetDxObjectDebugName(rayTracingPso->GetD3D12StateObject(), proxy.DebugName);

        pso = std::move(rayTracingPso);
    }

    void PsoManager::DestroyPso(uint32_t index)
    {
        BenzinAssert(index < m_Psos.size());

        m_Psos[index].reset();
    }

    Pso& PsoManager::GetPso(uint32_t index)
    {
        BenzinAssert(index < m_Psos.size());

        auto* pso = m_Psos[index].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<Pso*>(pso) != nullptr);

        return *(Pso*)m_Psos[index].get();
    }

    RayTracing_Pso& PsoManager::GetRayTracingPso(uint32_t index)
    {
        BenzinAssert(index < m_Psos.size());

        auto* pso = m_Psos[index].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<RayTracing_Pso*>(pso) != nullptr);

        return *(RayTracing_Pso*)pso;
    }

    void PsoManager::RecompilePsoCallback()
    {
        for (std::unique_ptr<PsoBase>& pso : m_Psos)
        {
            if (pso.get() == nullptr)
            {
                continue;
            }

            bool isPsoNeedsRecompilation = false;
            for (const auto& shader : pso->GetShaders())
            {
                isPsoNeedsRecompilation |= m_ShaderManager.CompareWithNewShader(shader);
                if (!isPsoNeedsRecompilation)
                {
                    continue;
                }

                const auto bytecode = m_ShaderManager.GetShaderBytecode(shader);
                if (bytecode.empty())
                {
                    return;
                }

                switch (shader.GetType())
                {
                    case ShaderType::Vertex:
                    {
                        ((GraphicsPso*)pso.get())->ChangeVs(bytecode);
                        break;
                    }
                    case ShaderType::Pixel:
                    {
                        ((GraphicsPso*)pso.get())->ChangePs(bytecode);
                        break;
                    }
                    case ShaderType::Compute:
                    {
                        ((ComputePso*)pso.get())->ChangeCs(bytecode);
                        break;
                    }
                    case ShaderType::Library:
                    {
                        ((RayTracing_Pso*)pso.get())->ChangeShaderLibrary(bytecode);
                        break;
                    }
                }
            }

            if (isPsoNeedsRecompilation)
            {
                pso->Compile();
            }
        }
    }

}
