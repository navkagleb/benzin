#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/cmd_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

namespace sandbox
{

    void TlasBuildingPass::OnUpdate()
    {
        BenzinProfile();

        // Note: Before updating TopLevel AccelerationStructure the TransformComponents must be updated
        ms_RayTracingScene->UpdateTlasBuffers();
    }

    void TlasBuildingPass::OnRender() const
    {
        BenzinProfile();
        BenzinGpuProfile("TlasBuilding");

        benzin::ComputeCmdList& cmdList = ms_Device->GetGraphicsCmdQueue().GetCmdList();

        const benzin::RayTracing_Tlas& tlas = ms_RayTracingScene->GetActiveTlas();

        BenzinScopedResourceBarriers(
            cmdList,
            benzin::TransitionBarrier{ *tlas.GetScratchResource(), benzin::ResourceState::UnorderedAccess }
        );

        cmdList.BuildRayTracingAccelerationStructure(tlas);
        cmdList.SetComputeSrv(benzin::UnifiedRootParameter::SceneTlas, tlas.GetGpuVirtualAddress());
    }

}
