#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"

#include <benzin/engine/scene.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/gpu_timer.hpp>

namespace sandbox
{

    TlasBuildingPass::TlasBuildingPass(benzin::Device& device, benzin::Scene& scene)
        : m_Device{ device }
        , m_Scene{ scene }
    {}

    void TlasBuildingPass::OnRender() const
    {
        // Before updating TopLevel AccelerationStructure the TransformComponents must be updated

        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinPushGpuEvent(commandList, "TlasBuildingPass");

        m_Scene.BuildTopLevelAccelerationStructure();
    }

}
