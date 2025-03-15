#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class ComputePso;
    class Device;
    class GraphicsPso;
    class PsoBase;
    class RayTracing_Pso;
    class ShaderManager;
    
    struct GraphicsInputElement;

    struct GraphicsPsoProxy
    {
        std::string DebugName;

        std::vector<GraphicsInputElement> InputLayout;

        std::string_view VsFileName;
        std::string_view VsEntryPoint = "VsMain";
        std::vector<std::string_view> VsDefines;

        std::string_view PsFileName;
        std::string_view PsEntryPoint = "PsMain";
        std::vector<std::string_view> PsDefines;

        PrimitiveTopologyType PrimitiveTopologyType = PrimitiveTopologyType::Unknown;
        RasterizerState RasterizerState;

        DepthState DepthState;
        StencilState StencilState;

        std::vector<GraphicsFormat> RenderTargetFormats;
        GraphicsFormat DepthStencilFormat = GraphicsFormat::Unknown;
        BlendState BlendState;
    };

    struct ComputePsoProxy
    {
        std::string_view DebugName;

        std::string_view CsFileName;
        std::string_view CsEntryPoint = "CsMain";
        std::vector<std::string_view> CsDefines;
    };

    struct RayTracing_PsoProxy
    {
        std::string_view DebugName;

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
            Bytes32 PayloadSize;
            Bytes32 AttributeSize;
        } ShaderConfig;
    };

    enum class PsoId : uint32_t;

    class PsoManager
    {
    public:
        using GraphicsPsoConfigurator = std::function<void(GraphicsPsoProxy& proxy)>;
        using ComputePsoConfigurator = std::function<void(ComputePsoProxy& proxy)>;
        using RayTracingPsoConfigurator = std::function<void(RayTracing_PsoProxy& proxy)>;

        PsoManager(Device& device, ShaderManager& shaderManager);
        ~PsoManager();

        void Create(PsoId id, const GraphicsPsoConfigurator& configurator);
        void Create(PsoId id, const ComputePsoConfigurator& configurator);
        void Create(PsoId id, const RayTracingPsoConfigurator& configurator);
        void Destroy(PsoId id);

        const GraphicsPso& GetGraphics(PsoId id) const;
        const ComputePso& GetCompute(PsoId id) const;
        const RayTracing_Pso& GetRayTracing(PsoId id) const;

    private:
        void RecompilePsoCallback();

    private:
        Device& m_Device;
        ShaderManager& m_ShaderManager;

        std::vector<std::unique_ptr<PsoBase>> m_Psos; // TODO: Pointer tagging can be used there to remove 3 different getters
    };

}
