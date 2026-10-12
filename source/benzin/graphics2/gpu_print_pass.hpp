#pragma once

#include <benzin/graphics2/imgui_pass.hpp>
#include <benzin/graphics2/render_pass.hpp>
#include <shaders/joint/gpu_print_resources.hpp>

namespace benzin
{

    class Buffer;
    class CopyCmdList;

    struct GpuPrintData
    {
        DirectX::XMINT2 m_CursorPosition = {};
        std::vector<std::string> m_PrintRecords;
    };

    class GpuPrintPass : public RenderPass
    {
    public:
        explicit GpuPrintPass(GpuPrintData& printData);

        bool IsDependentOnViewport() const override { return true; }

        void OnUpdate() override;
        void OnRender() const override;

        void ReadbackFromGpu(CopyCmdList& cmdList) const;

    private:
        GpuPrintData& m_PrintData;

        std::unique_ptr<Buffer> m_UavBuffer;
        std::unique_ptr<Buffer> m_ReadbackBuffer;

        joint::GpuPrintConsts m_Consts = {};
    };

    class GpuPrintTool : public ImGuiTool
    {
    public:
        explicit GpuPrintTool(const GpuPrintData& printData);

        void DrawWindowContent() override;

    private:
        const GpuPrintData& m_PrintData;
    };

}
