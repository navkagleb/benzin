#pragma once

#include <benzin/graphics2/render_pass.hpp>

namespace benzin
{
    class Buffer;
    class QueryHeap;
    class Texture;

    enum class PsoId : uint32_t;
}

namespace sandbox
{

    class GeometryPass : public benzin::RenderPass
    {
    public:
        GeometryPass();
        ~GeometryPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnZeroFrameInit() override;
        void OnRenderViewportResize() override;
        void OnRender() const override;

    private:
        static void CreateGeometryPso(benzin::PsoId id, bool isMeshPipeline);

        void RunCullingPass(bool isLate) const;
        void RunDrawPass(bool isLate) const;
        void RunHzbGeneration() const;

        std::unique_ptr<benzin::Buffer> m_VisibilityBuffer;
        std::unique_ptr<benzin::Buffer> m_CmdCountBuffers[BENZIN_FRAME_COUNT];
        std::unique_ptr<benzin::Buffer> m_DrawCmdBuffers[BENZIN_FRAME_COUNT];
        std::unique_ptr<benzin::Buffer> m_DispatchCmdBuffers[BENZIN_FRAME_COUNT];

        std::unique_ptr<benzin::QueryHeap> m_StatsQueryHeap;
        std::unique_ptr<benzin::Buffer> m_StatsBuffer;

        ID3D12CommandSignature* m_D3D12DrawCmdSignature = nullptr;
        ID3D12CommandSignature* m_D3D12MeshDispatchCmdSignature = nullptr;
    };

    struct GBuffer
    {
        const benzin::Texture& m_AlbedoAndRoughness;
        const benzin::Texture& m_EmissiveAndMetallic;
        const benzin::Texture& m_WorldNormal;
        const benzin::Texture& m_Mv;
        const benzin::Texture& m_ViewDepth;
        const benzin::Texture& m_Depth;

        explicit GBuffer(const benzin::RenderResources& resources);
    };

}
