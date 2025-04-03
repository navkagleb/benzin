#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics2/pso_manager.hpp"

#include "benzin/graphics/d3d12_utils.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/pso.hpp"
#include "benzin/graphics/ray_tracing_pso.hpp"
#include "benzin/graphics2/game_specific_resource_ids.hpp"
#include "benzin/graphics2/shader_manager.hpp"

namespace benzin
{

    PsoManager::PsoManager(Device& device, ShaderManager& shaderManager)
        : m_Device{ device }
        , m_ShaderManager{ shaderManager }
    {
        m_Psos.resize(magic_enum::enum_count<PsoId>());

        m_ShaderManager.SetNewShaderAvailableCallback([this] { RecompilePsoCallback(); });
    }

    PsoManager::~PsoManager()
    {
#if BENZIN_IS_ASSERTS_ENABLED
        uint32_t aliveCount = 0;
        for (const auto& pso : m_Psos)
        {
            aliveCount += (pso.get() != nullptr);
        }

        BenzinAssert(aliveCount == 0, "Not all PSOs are released properly. Alive PSO count: {}", aliveCount);
#endif
    }

    void PsoManager::Create(PsoId id, const GraphicsPsoConfigurator& configurator)
    {
        BenzinAssert(magic_enum::enum_contains(id));
        BenzinAssert((bool)configurator);

        GraphicsPsoProxy proxy;
        configurator(proxy);

        BenzinAssert(!proxy.VsFileName.empty());
        if (!proxy.RenderTargetFormats.empty())
        {
            BenzinAssert(!proxy.PsFileName.empty());
        }

        auto& pso = m_Psos[+id];
        BenzinAssert(pso.get() == nullptr);

        ShaderInfo vs{ ShaderType::Vertex, proxy.VsFileName, proxy.VsEntryPoint, std::move(proxy.VsDefines) };
        ShaderInfo ps{ ShaderType::Pixel, proxy.PsFileName, proxy.PsEntryPoint, std::move(proxy.PsDefines) };

        auto graphicsPso = std::make_unique<GraphicsPso>(m_Device);
        
        if (!proxy.InputLayout.empty())
        {
            graphicsPso->SetInputLayout(proxy.InputLayout);
        }
        
        const auto vsBytecode = m_ShaderManager.GetShaderBytecode(vs);
        graphicsPso->SetVs(std::move(vs), vsBytecode);

        if (ps.IsValid())
        {
            const auto psBytecode = m_ShaderManager.GetShaderBytecode(ps);
            graphicsPso->SetPs(std::move(ps), psBytecode);
        }

        graphicsPso->SetPrimitiveTopologyType(proxy.PrimitiveTopologyType);
        graphicsPso->SetRasterizerState(proxy.RasterizerState);
        graphicsPso->SetDepthStencilState(proxy.DepthState, proxy.StencilState);
        graphicsPso->SetBlendState(proxy.BlendState);
        graphicsPso->SetRenderTargetFormats(proxy.RenderTargetFormats);
        graphicsPso->SetDepthStencilFormat(proxy.DepthStencilFormat);
        graphicsPso->Compile();

        SetDxObjectDebugName(graphicsPso->GetD3D12PipelineState(), magic_enum::enum_name(id));

        pso = std::move(graphicsPso);
    }

    void PsoManager::Create(PsoId id, const ComputePsoConfigurator& configurator)
    {
        BenzinAssert(magic_enum::enum_contains(id));
        BenzinAssert((bool)configurator);

        ComputePsoProxy proxy;
        configurator(proxy);

        BenzinAssert(!proxy.CsFileName.empty());

        auto& pso = m_Psos[+id];
        BenzinAssert(pso.get() == nullptr);

        ShaderInfo cs{ ShaderType::Compute, proxy.CsFileName, proxy.CsEntryPoint, std::move(proxy.CsDefines) };
        const auto csBytecode = m_ShaderManager.GetShaderBytecode(cs);

        auto computePso = std::make_unique<ComputePso>(m_Device);
        computePso->SetCs(std::move(cs), csBytecode);
        computePso->Compile();

        SetDxObjectDebugName(computePso->GetD3D12PipelineState(), magic_enum::enum_name(id));

        pso = std::move(computePso);
    }

    void PsoManager::Create(PsoId id, const RayTracingPsoConfigurator& configurator)
    {
        BenzinAssert(magic_enum::enum_contains(id));
        BenzinAssert((bool)configurator);

        RayTracing_PsoProxy proxy;
        configurator(proxy);

        BenzinAssert(!proxy.ShaderLibrary.FileName.empty());
        // TODO: Add more checks for mandatory entry points

        auto& pso = m_Psos[+id];
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

        SetDxObjectDebugName(rayTracingPso->GetD3D12StateObject(), magic_enum::enum_name(id));

        pso = std::move(rayTracingPso);
    }

    void PsoManager::Destroy(PsoId id)
    {
        BenzinAssert(magic_enum::enum_contains(id));

        m_Psos[+id].reset();
    }

    const GraphicsPso& PsoManager::GetGraphics(PsoId id) const
    {
        BenzinAssert(magic_enum::enum_contains(id));

        auto* pso = m_Psos[+id].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<GraphicsPso*>(pso) != nullptr);

        return *(const GraphicsPso*)pso;
    }

    const ComputePso& PsoManager::GetCompute(PsoId id) const
    {
        BenzinAssert(magic_enum::enum_contains(id));

        auto* pso = m_Psos[+id].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<ComputePso*>(pso) != nullptr);

        return *(ComputePso*)pso;
    }

    const RayTracing_Pso& PsoManager::GetRayTracing(PsoId id) const
    {
        BenzinAssert(magic_enum::enum_contains(id));

        auto* pso = m_Psos[+id].get();
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
                pso->Release();
                pso->Compile();
            }
        }
    }

}
