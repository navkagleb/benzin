#pragma once

#include "benzin/graphics/render_states.hpp"

namespace benzin
{

    class Device;
    class Pso;
    class PsoBase;
    class RayTracing_Pso;
    class ShaderManager;

    struct GraphicsPsoProxy
    {
        std::string DebugName;

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

    class PsoManager
    {
    public:
        using GraphicsPsoConfigurator = std::function<void(GraphicsPsoProxy&)>;
        using ComputePsoConfigurator = std::function<void(ComputePsoProxy&)>;
        using RayTracingPsoConfigurator = std::function<void(RayTracing_PsoProxy&)>;

        PsoManager(Device& device, ShaderManager& shaderManager, uint32_t psoCount);
        ~PsoManager();

        void CreateGraphicsPso(uint32_t index, const GraphicsPsoConfigurator& configurator);
        void CreateComputePso(uint32_t index, const ComputePsoConfigurator& configurator);
        void CreateRayTracingPso(uint32_t index, const RayTracingPsoConfigurator& configurator);
        void DestroyPso(uint32_t index);

        Pso& GetPso(uint32_t index);
        RayTracing_Pso& GetRayTracingPso(uint32_t index);

    private:
        void RecompilePsoCallback();

    private:
        Device& m_Device;
        ShaderManager& m_ShaderManager;

        std::vector<std::unique_ptr<PsoBase>> m_Psos;
    };

}
