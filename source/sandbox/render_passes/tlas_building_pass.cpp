#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

namespace sandbox
{

    TlasBuildingPass::TlasBuildingPass(benzin::Device& device, benzin::RayTracing_Scene& rayTracingScene)
        : m_Device{ device }
        , m_RayTracingScene{ rayTracingScene }
    {}

    void TlasBuildingPass::OnRender() const
    {
        BenzinProfile();

        // Before updating TopLevel AccelerationStructure the TransformComponents must be updated

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "TlasBuilding");

        const uint64_t tlasGpuAddress = m_RayTracingScene.BuildTlas();
        commandList.SetSrv(benzin::UnifiedRootParameter::SceneTlas, tlasGpuAddress);
    }

}
