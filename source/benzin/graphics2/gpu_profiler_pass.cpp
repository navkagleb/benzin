#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler_pass.hpp>

#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

namespace benzin
{

    void GpuProfilerPass::OnRender() const
    {
        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        auto& timestampQueryHeap = ms_GpuProfiler->GetTimestampQueryHeap();

        BenzinGpuEvent(commandList, "ResolveTimestamps");

        ms_GpuProfiler->ForceProfileUnprofiledTimestamps([&commandList, &timestampQueryHeap](uint32_t timestampIndex)
        {
            commandList.SetTimestamp(timestampQueryHeap, timestampIndex);
        });

        commandList.ResolveTimestamps(timestampQueryHeap, ms_GpuProfiler->GetReadbackBuffer(), ms_GpuProfiler->GetResolveReadbackBufferOffset());
    }

}
