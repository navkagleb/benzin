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

        struct TempDrawRangeBatch
        {
            std::vector<DirectX::XMMATRIX> m_LocalToWorldMatrices;
            std::vector<DirectX::XMMATRIX> m_PrevLocalToWorldMatrices;
            std::vector<uint32_t> m_MaterialIndices;
        };

        struct DrawRangeBatch
        {
            benzin::SubRange32 m_InstanceRange;
        };

        struct DrawRangeBatchGpuStorage
        {
            std::unique_ptr<benzin::Buffer> m_LocalToWorldMatrixBuffer;
            std::unique_ptr<benzin::Buffer> m_PrevLocalToWorldMatrixBuffer;
            std::unique_ptr<benzin::Buffer> m_MaterialIndexBuffer;
        };

        template <typename DrawRangeBatchT>
        using MeshBatch = std::unordered_map<uint32_t, DrawRangeBatchT>;

        using TempMeshBatches = std::unordered_map<entt::entity, MeshBatch<TempDrawRangeBatch>>;
        using MeshBatches = std::unordered_map<entt::entity, MeshBatch<DrawRangeBatch>>;

        bool IsDependentOnViewport() const override { return true; }

        void OnRenderViewportResize() override;
        void OnUpdate() override;
        void OnRender() const override;

        void CreateGeometryPso(benzin::PsoId id, benzin::EnumFlags<PsoFlag> flags = {});

        void CreateMeshBatches();
        void AddToTempMeshBatches(const benzin::MeshComponent& meshComponent, const benzin::Transform& localToWorldMatrix);
        void ProcessTempMeshBatches(TempMeshBatches& tempMeshBatches, MeshBatches& outMeshBatches);
        void RenderMeshBatches(benzin::GraphicsCmdList& cmdList, const MeshBatches& meshBatches) const;

        void ReprojectDepth(benzin::ComputeCmdList& cmdList) const;
        void GenerateHzb(benzin::ComputeCmdList& cmdList) const;
        void RunColorPass(benzin::GraphicsCmdList& cmdList) const;

    private:
        joint::GeometryPassConsts m_Consts = {};

        TempMeshBatches m_TempOpaqueMeshBatches;
        TempMeshBatches m_TempAlphaMeshBatches;

        MeshBatches m_OpaqueMeshBatches;
        MeshBatches m_AlphaMeshBatches;

        uint32_t m_TotalInstanceCount = 0;
        uint32_t m_InstanceOffset = 0;

        DrawRangeBatchGpuStorage m_BatchStorage;

        bool m_IsAmplificationDispatchUsed = true;
    };

}
