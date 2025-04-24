#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class ComputePso;
    class Device;
    class MeshPso;
    class PsoBase;
    class RayTracing_Pso;
    class ShaderManager;
    class VertexPso;
    
    struct VertexInputElement;

    struct ShaderProxy
    {
        std::string_view FileName;
        std::string_view EntryPoint;
        std::vector<std::string_view> Defines;

        explicit ShaderProxy(std::string_view entryPoint)
            : EntryPoint{ entryPoint }
        {}
    };

    struct GraphicsPsoProxy
    {
        ShaderProxy Ps{ "PsMain" };
        RasterizerState RasterizerState;
        DepthState DepthState
        {
            .IsEnabled = false,
            .IsWriteEnabled = false,
        };
        StencilState StencilState;
        std::vector<GraphicsFormat> RenderTargetFormats;
        GraphicsFormat DepthStencilFormat = GraphicsFormat::Unknown;
        BlendState BlendState;
    };

    struct VertexPsoProxy : GraphicsPsoProxy
    {
        ShaderProxy Vs{ "VsMain" };
        std::vector<VertexInputElement> InputLayout;
        PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Unknown;
    };

    struct MeshPsoProxy : GraphicsPsoProxy
    {
        ShaderProxy As{ "AsMain" };
        ShaderProxy Ms{ "MsMain" };
    };

    struct ComputePsoProxy
    {
        ShaderProxy Cs{ "CsMain" };
    };

    struct RayTracing_PsoProxy
    {
        struct
        {
            std::string_view FileName;
            std::vector<std::string_view> Defines;
        } ShaderLibrary;

        std::string_view RayGenerationEntryPoint;
        std::string_view MissEntryPoint;

        struct
        {
            std::string_view Name;
            std::string_view ClosestHitEntryPoint;
        } HitGroup;

        struct
        {
            uint32_t PayloadSizeInBytes;
            uint32_t AttributeSizeInBytes;
        } ShaderConfig;
    };

    class PsoManager
    {
    public:
        using VertexPsoConfigurator = std::function<void(VertexPsoProxy& proxy)>;
        using MeshPsoConfigurator = std::function<void(MeshPsoProxy& proxy)>;
        using ComputePsoConfigurator = std::function<void(ComputePsoProxy& proxy)>;
        using RayTracingPsoConfigurator = std::function<void(RayTracing_PsoProxy& proxy)>;

        PsoManager(Device& device, ShaderManager& shaderManager);
        ~PsoManager();

        void Create(PsoId id, const VertexPsoConfigurator& configurator);
        void Create(PsoId id, const MeshPsoConfigurator& configurator);
        void Create(PsoId id, const ComputePsoConfigurator& configurator);
        void Create(PsoId id, const RayTracingPsoConfigurator& configurator);
        void Destroy(PsoId id);

        const VertexPso& GetVertex(PsoId id) const;
        const MeshPso& GetMesh(PsoId id) const;
        const ComputePso& GetCompute(PsoId id) const;
        const RayTracing_Pso& GetRayTracing(PsoId id) const;

    private:
        template <typename PsoT>
        using PsoCreator = std::function<void(PsoT& pso)>;

        template <typename PsoT>
        void Create(PsoId id, const PsoCreator<PsoT>& creator);

        template <typename PsoT>
        const PsoT& Get(PsoId id) const;

        void RecompilePsoCallback();

    private:
        Device& m_Device;
        ShaderManager& m_ShaderManager;

        std::vector<std::unique_ptr<PsoBase>> m_Psos;
    };

}
