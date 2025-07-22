#include <benzin/config/bootstrap.hpp>
#include <benzin/tools/vram_tool.hpp>

#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_heap.hpp>

namespace benzin
{

    VramTool::VramTool(Device& device)
        : ImGuiTool{ "Graphics/Vram" }
        , m_Device{ device }
    {}

    void VramTool::DrawWindowContent()
    {
        {
            const GpuHeapLinearBufferAllocator& allocator = m_Device.GetPersistentLinearBufferAllocator();

            ImGui::SeparatorText("PERSISTENT ALLOCATOR");
            ImGui::FmtText("Allocated: {:.0f}/{:.0f} mb", ToMb(allocator.GetOffsetInBytes()), ToMb(allocator.GetSizeInBytes()));
        }
        
        {
            const GpuHeapLinearBufferAllocator& allocator = m_Device.GetPrevTemporalLinearBufferAllocator();

            ImGui::SeparatorText("TEMPORAL ALLOCATOR");
            ImGui::FmtText("Allocated: {:.0f}/{:.0f} kb", ToKb(allocator.GetOffsetInBytes()), ToKb(allocator.GetSizeInBytes()));
        }
    }

}
