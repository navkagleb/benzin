#pragma once

#include <benzin/graphics/common.hpp>

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
        std::string_view m_FileName;
        std::string_view m_EntryPoint;
        std::vector<std::string_view> m_Defines;

        explicit ShaderProxy(std::string_view entryPoint)
            : m_EntryPoint{ entryPoint }
        {}
    };

    struct GraphicsPsoProxy
    {
        ShaderProxy m_Ps{ "PsMain" };
        RasterizerState m_RasterizerState;
        DepthState m_DepthState;
        std::vector<DXGI_FORMAT> m_RenderTargetDxgiFormats;
        DXGI_FORMAT m_DepthStencilDxgiFormat = DXGI_FORMAT_UNKNOWN;
        BlendState m_BlendState;
    };

    struct VertexPsoProxy : GraphicsPsoProxy
    {
        ShaderProxy m_Vs{ "VsMain" };
        std::vector<VertexInputElement> m_InputLayout;
    };

    struct MeshPsoProxy : GraphicsPsoProxy
    {
        ShaderProxy m_As{ "AsMain" };
        ShaderProxy m_Ms{ "MsMain" };
    };

    struct ComputePsoProxy
    {
        ShaderProxy m_Cs{ "CsMain" };
    };

    struct RayTracing_PsoProxy
    {
        struct
        {
            std::string_view m_FileName;
            std::vector<std::string_view> m_Defines;
        } m_ShaderLibrary;

        std::string_view m_RayGenerationEntryPoint;
        std::string_view m_MissEntryPoint;

        struct
        {
            std::string_view m_Name;
            std::string_view m_ClosestHitEntryPoint;
        } m_HitGroup;

        struct
        {
            uint32_t m_PayloadSizeInBytes;
            uint32_t m_AttributeSizeInBytes;
        } m_ShaderConfig;
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

        Device& m_Device;
        ShaderManager& m_ShaderManager;

        std::vector<std::unique_ptr<PsoBase>> m_Psos;
    };

}
