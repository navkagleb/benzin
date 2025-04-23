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

    class PsoBaseWrapper
    {
    public:
        using PsoBasePtrRef = std::unique_ptr<PsoBase>&;

        PsoBaseWrapper(PsoBasePtrRef pso)
            : m_Pso{ pso }
        {}

        template <std::derived_from<PsoBase> PsoT>
        PsoT& GetAs()
        {
            return *(PsoT*)m_Pso.get();
        }

        template <std::derived_from<PsoBase> PsoT>
        PsoT* GetAsPtr()
        {
            return dynamic_cast<PsoT*>(m_Pso.get());
        }

    private:
        PsoBasePtrRef m_Pso;
    };

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    static void CreateGraphicsPso(
        GraphicsPsoProxy& proxy,
        ShaderManager& shaderManager,
        GraphicsPso<PsoStreamT, _MaxShaderCount>& pso
    )
    {
        ShaderInfo ps{ ShaderType::Pixel, proxy.Ps.FileName, proxy.Ps.EntryPoint, std::move(proxy.Ps.Defines) };

        if (!proxy.RenderTargetFormats.empty())
        {
            BenzinAssert(ps.IsValid());
        }

        if (proxy.DepthStencilFormat == GraphicsFormat::Unknown)
        {
            BenzinAssert(!proxy.DepthState.IsEnabled && !proxy.DepthState.IsWriteEnabled);
        }
        else
        {
            BenzinAssert(proxy.DepthState.IsEnabled);
        }

        if (ps.IsValid())
        {
            const auto psBytecode = shaderManager.GetShaderBytecode(ps);
            pso.SetPs(std::move(ps), psBytecode);
        }

        pso.SetRasterizerState(proxy.RasterizerState);
        pso.SetDepthStencilState(proxy.DepthState, proxy.StencilState);
        pso.SetBlendState(proxy.BlendState);
        pso.SetRenderTargetFormats(proxy.RenderTargetFormats);
        pso.SetDepthStencilFormat(proxy.DepthStencilFormat);
    }

    //

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

    void PsoManager::Create(PsoId id, const VertexPsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        VertexPsoProxy proxy;
        configurator(proxy);

        Create<VertexPso>(id, [this, &proxy](VertexPso& pso)
        {
            CreateGraphicsPso(proxy, m_ShaderManager, pso);

            ShaderInfo vs{ ShaderType::Vertex, proxy.Vs.FileName, proxy.Vs.EntryPoint, std::move(proxy.Vs.Defines) };
            BenzinAssert(vs.IsValid());

            const auto vsBytecode = m_ShaderManager.GetShaderBytecode(vs);
            pso.SetVs(std::move(vs), vsBytecode);

            if (!proxy.InputLayout.empty())
            {
                pso.SetInputLayout(proxy.InputLayout);
            }

            pso.SetPrimitiveTopologyType(proxy.PrimitiveTopologyType);
        });
    }

    void PsoManager::Create(PsoId id, const MeshPsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        MeshPsoProxy proxy;
        configurator(proxy);

        Create<MeshPso>(id, [this, &proxy](MeshPso& pso)
        {
            CreateGraphicsPso(proxy, m_ShaderManager, pso);

            ShaderInfo ms{ ShaderType::Mesh, proxy.Ms.FileName, proxy.Ms.EntryPoint, std::move(proxy.Ms.Defines) };
            BenzinAssert(ms.IsValid());

            const auto msBytecode = m_ShaderManager.GetShaderBytecode(ms);
            pso.SetMs(std::move(ms), msBytecode);
        });
    }

    void PsoManager::Create(PsoId id, const ComputePsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        ComputePsoProxy proxy;
        configurator(proxy);

        Create<ComputePso>(id, [this, &proxy](ComputePso& pso)
        {
            ShaderInfo cs{ ShaderType::Compute, proxy.Cs.FileName, proxy.Cs.EntryPoint, std::move(proxy.Cs.Defines) };
            BenzinAssert(cs.IsValid());

            const auto csBytecode = m_ShaderManager.GetShaderBytecode(cs);
            pso.SetCs(std::move(cs), csBytecode);
        });
    }

    void PsoManager::Create(PsoId id, const RayTracingPsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        RayTracing_PsoProxy proxy;
        configurator(proxy);

        Create<RayTracing_Pso>(id, [this, &proxy](RayTracing_Pso& pso)
        {
            // TODO: Add more checks for mandatory entry points

            ShaderInfo library{ ShaderType::Library, proxy.ShaderLibrary.FileName, {}, std::move(proxy.ShaderLibrary.Defines) };
            BenzinAssert(library.IsValid());

            const auto libraryBytecode = m_ShaderManager.GetShaderBytecode(library);
            pso.SetShaderLibrary(std::move(library), libraryBytecode);

            pso.SetRayGenerationShader(proxy.RayGenerationEntryPoint);
            pso.SetMissShader(proxy.MissEntryPoint);
            pso.SetHitGroup(proxy.HitGroup.Name, proxy.HitGroup.ClosestHitEntryPoint);
            pso.SetShaderConfig(proxy.ShaderConfig.PayloadSize, proxy.ShaderConfig.AttributeSize);
        });
    }

    void PsoManager::Destroy(PsoId id)
    {
        BenzinAssert(magic_enum::enum_contains(id));

        m_Psos[+id].reset();
    }

    const VertexPso& PsoManager::GetVertex(PsoId id) const
    {
        return Get<VertexPso>(id);
    }

    const MeshPso& PsoManager::GetMesh(PsoId id) const
    {
        return Get<MeshPso>(id);
    }

    const ComputePso& PsoManager::GetCompute(PsoId id) const
    {
        return Get<ComputePso>(id);
    }

    const RayTracing_Pso& PsoManager::GetRayTracing(PsoId id) const
    {
        return Get<RayTracing_Pso>(id);
    }

    template <typename PsoT>
    void PsoManager::Create(PsoId id, const PsoCreator<PsoT>& creator)
    {
        BenzinAssert(magic_enum::enum_contains(id));

        auto& pso = m_Psos[+id];
        BenzinAssert(pso.get() == nullptr);

        auto psoT = std::make_unique<PsoT>(m_Device);
        creator(*psoT);

        psoT->Compile(magic_enum::enum_name(id));

        pso = std::move(psoT);
    }

    template <typename PsoT>
    const PsoT& PsoManager::Get(PsoId id) const
    {
        BenzinAssert(magic_enum::enum_contains(id));

        auto* pso = m_Psos[+id].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<PsoT*>(pso) != nullptr);

        return *(PsoT*)pso;
    }

    void PsoManager::RecompilePsoCallback()
    {
        for (uint32_t rawId = 0; rawId < m_Psos.size(); ++rawId)
        {
            std::unique_ptr<PsoBase>& pso = m_Psos[rawId];

            if (pso.get() == nullptr)
            {
                continue;
            }

            PsoBaseWrapper psoWrapper = pso;

            bool isPsoNeedsRecompilation = false;
            for (const ShaderInfo& shader : pso->GetShaders())
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
                        psoWrapper.GetAs<VertexPso>().ChangeVs(bytecode);
                        break;
                    }
                    case ShaderType::Mesh:
                    {
                        psoWrapper.GetAs<MeshPso>().ChangeMs(bytecode);
                        break;
                    }
                    case ShaderType::Pixel:
                    {
                        if (auto* vertexPso = psoWrapper.GetAsPtr<VertexPso>(); vertexPso != nullptr)
                        {
                            vertexPso->ChangePs(bytecode);
                            break;
                        }

                        auto* meshPso = psoWrapper.GetAsPtr<MeshPso>();
                        BenzinAssert(meshPso != nullptr);

                        meshPso->ChangePs(bytecode);

                        break;
                    }
                    case ShaderType::Compute:
                    {
                        psoWrapper.GetAs<ComputePso>().ChangeCs(bytecode);
                        break;
                    }
                    case ShaderType::Library:
                    {
                        psoWrapper.GetAs<RayTracing_Pso>().ChangeShaderLibrary(bytecode);
                        break;
                    }
                    default:
                    {
                        BenzinAssert(false, "Missing ShaderType: {}", magic_enum::enum_name(shader.GetType()));
                        break;
                    }
                }
            }

            if (isPsoNeedsRecompilation)
            {
                pso->Release();
                pso->Compile(magic_enum::enum_name(PsoId{ rawId }));
            }
        }
    }

}
