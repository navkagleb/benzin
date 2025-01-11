#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"

#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>
#include <benzin/graphics/unified_root_signature.hpp>

namespace sandbox
{

    TlasBuildingPass::TlasBuildingPass(benzin::Device& device, benzin::RayTracingScene& rayTracingScene)
        : m_Device{ device }
        , m_RayTracingScene{ rayTracingScene }
    {}

    void TlasBuildingPass::OnRender() const
    {
        // Before updating TopLevel AccelerationStructure the TransformComponents must be updated

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "TlasBuildingPass");

        const uint64_t tlasGpuAddress = m_RayTracingScene.BuildTlas();
        commandList.SetSrv(benzin::UnifiedRootParameter::TopLevelAs, tlasGpuAddress);
    }

}
