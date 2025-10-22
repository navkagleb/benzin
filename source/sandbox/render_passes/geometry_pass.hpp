#pragma once

#include <benzin/graphics2/render_pass.hpp>

#include <shaders/joint/geometry_resources.hpp>

namespace benzin
{
    struct Mesh;

    class ComputeCmdList;
    class GraphicsCmdList;
    class MeshComponent;
    class Transform;

    enum class PsoId : uint32_t;
}

namespace sandbox
{

    struct GBuffer;
    struct GBufferSettings;
    struct GBufferStats;

    class GeometryPass : public benzin::RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override;

    private:
        enum class PsoFlag
        {
            AlphaTest,
            MeshPipeline,
        };

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        void CreateGeometryPso(benzin::PsoId id, benzin::EnumFlags<PsoFlag> flags = {});

        void ReprojectDepth(benzin::ComputeCmdList& cmdList) const;
        void GenerateHzb(benzin::ComputeCmdList& cmdList) const;
        void RunColorPass(benzin::GraphicsCmdList& cmdList) const;

    private:
        joint::GeometryPassConsts m_Consts = {};

        bool m_IsAmplificationDispatchUsed = false;
    };

}
