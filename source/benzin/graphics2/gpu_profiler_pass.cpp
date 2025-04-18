#include <benzin/config/bootstrap.hpp>
#include <benzin/graphics2/gpu_profiler_pass.hpp>

#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

namespace benzin
{

    void GpuProfilerPass::OnRender() const
    {
        auto& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();
        auto& timestampQueryHeap = ms_GpuProfiler->GetTimestampQueryHeap();

        BenzinGpuEvent(cmdList, "ResolveTimestamps");

        ms_GpuProfiler->ForceProfileUnprofiledTimestamps([&cmdList, &timestampQueryHeap](uint32_t timestampIndex)
        {
            cmdList.SetTimestamp(timestampQueryHeap, timestampIndex);
        });

        cmdList.ResolveTimestamps(timestampQueryHeap, ms_GpuProfiler->GetReadbackBuffer(), ms_GpuProfiler->GetResolveReadbackOffsetInBytes());
    }

}
