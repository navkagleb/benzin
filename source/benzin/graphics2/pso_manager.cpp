#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/pso_manager.hpp>

#include <benzin/core/profiler.hpp>
#include <benzin/graphics/d3d12_utils.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/pso.hpp>
#include <benzin/graphics/ray_tracing_pso.hpp>
#include <benzin/graphics2/game_specific_resource_ids.hpp>
#include <benzin/graphics2/shader_manager.hpp>

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

    static ShaderInfo ShaderProxyToShaderInfo(ShaderType type, ShaderProxy&& proxy)
    {
        return ShaderInfo
        {
            type,
            proxy.m_FileName,
            proxy.m_EntryPoint,
            std::move(proxy.m_Defines),
        };
    }

    template <typename PsoStreamT, uint32_t _MaxShaderCount>
    static void CreateGraphicsPso(
        GraphicsPsoProxy& proxy,
        ShaderManager& shaderManager,
        GraphicsPso<PsoStreamT, _MaxShaderCount>& pso
    )
    {
        ShaderInfo ps = ShaderProxyToShaderInfo(ShaderType::Pixel, std::move(proxy.m_Ps));

        if (!proxy.m_RenderTargetDxgiFormats.empty())
        {
            BenzinAssert(ps.IsValid());
        }

        if (proxy.m_DepthStencilDxgiFormat == DXGI_FORMAT_UNKNOWN)
        {
            BenzinAssert(!proxy.m_DepthState.m_IsEnabled && !proxy.m_DepthState.m_IsWriteEnabled);
        }
        else
        {
            BenzinAssert(proxy.m_DepthState.m_IsEnabled);
        }

        if (ps.IsValid())
        {
            pso.SetPs(std::move(ps), shaderManager.GetShaderBytecode(ps));
        }

        pso.SetRasterizerState(proxy.m_RasterizerState);
        pso.SetDepthStencilState(proxy.m_DepthState);
        pso.SetBlendState(proxy.m_BlendState);
        pso.SetRenderTargetDxgiFormats(proxy.m_RenderTargetDxgiFormats);
        pso.SetDepthStencilDxgiFormat(proxy.m_DepthStencilDxgiFormat);
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

            ShaderInfo vs = ShaderProxyToShaderInfo(ShaderType::Vertex, std::move(proxy.m_Vs));

            BenzinAssert(vs.IsValid());
            pso.SetVs(std::move(vs), m_ShaderManager.GetShaderBytecode(vs));

            if (!proxy.m_InputLayout.empty())
            {
                pso.SetInputLayout(proxy.m_InputLayout);
            }
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

            ShaderInfo as = ShaderProxyToShaderInfo(ShaderType::Amplification, std::move(proxy.m_As));
            ShaderInfo ms = ShaderProxyToShaderInfo(ShaderType::Mesh, std::move(proxy.m_Ms));

            if (as.IsValid())
            {
                pso.SetAs(std::move(as), m_ShaderManager.GetShaderBytecode(as));
            }

            BenzinAssert(ms.IsValid());
            pso.SetMs(std::move(ms), m_ShaderManager.GetShaderBytecode(ms));
        });
    }

    void PsoManager::Create(PsoId id, const ComputePsoConfigurator& configurator)
    {
        BenzinAssert((bool)configurator);

        ComputePsoProxy proxy;
        configurator(proxy);

        Create<ComputePso>(id, [this, &proxy](ComputePso& pso)
        {
            ShaderInfo cs = ShaderProxyToShaderInfo(ShaderType::Compute, std::move(proxy.m_Cs));
            BenzinAssert(cs.IsValid());

            pso.SetCs(std::move(cs), m_ShaderManager.GetShaderBytecode(cs));
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

            ShaderInfo library{ ShaderType::Library, proxy.m_ShaderLibrary.m_FileName, {}, std::move(proxy.m_ShaderLibrary.m_Defines) };
            BenzinAssert(library.IsValid());

            pso.SetShaderLibrary(std::move(library), m_ShaderManager.GetShaderBytecode(library));
            pso.SetRayGenerationShader(proxy.m_RayGenerationEntryPoint);
            pso.SetMissShader(proxy.m_MissEntryPoint);
            pso.SetHitGroup(proxy.m_HitGroup.m_Name, proxy.m_HitGroup.m_ClosestHitEntryPoint);
            pso.SetShaderConfig(proxy.m_ShaderConfig.m_PayloadSizeInBytes, proxy.m_ShaderConfig.m_AttributeSizeInBytes);
        });
    }

    void PsoManager::Destroy(PsoId id)
    {
        BenzinAssert(magic_enum::enum_contains(id));

        m_Psos[*id].reset();
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

        auto& pso = m_Psos[*id];
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

        auto* pso = m_Psos[*id].get();
        BenzinAssert(pso != nullptr);
        BenzinAssert(dynamic_cast<PsoT*>(pso) != nullptr);

        return *(PsoT*)pso;
    }

    void PsoManager::RecompilePsoCallback()
    {
        BenzinProfile();

        for (uint32_t rawId = 0; rawId < m_Psos.size(); ++rawId)
        {
            std::unique_ptr<PsoBase>& pso = m_Psos[rawId];

            if (pso.get() == nullptr)
                continue;

            PsoBaseWrapper psoWrapper = pso;

            bool isPsoNeedsRecompilation = false;
            for (const ShaderInfo& shader : pso->GetShaders())
            {
                isPsoNeedsRecompilation |= m_ShaderManager.CompareWithNewShader(shader);
                if (!isPsoNeedsRecompilation)
                    continue;

                const auto bytecode = m_ShaderManager.GetShaderBytecode(shader);
                if (bytecode.empty())
                    return;

                switch (shader.GetType())
                {
                    case ShaderType::Vertex:
                    {
                        psoWrapper.GetAs<VertexPso>().ChangeVs(bytecode);
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
                    case ShaderType::Amplification:
                    {
                        psoWrapper.GetAs<MeshPso>().ChangeAs(bytecode);
                    }
                    case ShaderType::Mesh:
                    {
                        psoWrapper.GetAs<MeshPso>().ChangeMs(bytecode);
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
