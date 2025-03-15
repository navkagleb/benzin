#include "sandbox/bootstrap.hpp"
#include "sandbox/render_passes/tlas_building_pass.hpp"

#include <benzin/core/profiler.hpp>
#include <benzin/engine/ray_tracing_scene.hpp>
#include <benzin/engine/scene.hpp>
#include <benzin/graphics/buffer.hpp>
#include <benzin/graphics/command_list.hpp>
#include <benzin/graphics/command_queue.hpp>
#include <benzin/graphics/device.hpp>
#include <benzin/graphics/ray_tracing_acceleration_structures.hpp>
#include <benzin/graphics/unified_root_signature.hpp>
#include <benzin/graphics2/gpu_profiler.hpp>

namespace sandbox
{

    TlasBuildingPass::TlasBuildingPass(benzin::Device& device, benzin::RayTracing_Scene& rayTracingScene)
        : m_Device{ device }
        , m_RayTracingScene{ rayTracingScene }
    {}

    void TlasBuildingPass::OnUpdate()
    {
        BenzinProfile();

        // Note: Before updating TopLevel AccelerationStructure the TransformComponents must be updated
        m_RayTracingScene.UpdateTlasBuffers();
    }

    void TlasBuildingPass::OnRender() const
    {
        BenzinProfile();


        auto& commandList = ms_Device->GetGraphicsCommandQueue().GetCommandList();
        BenzinGpuProfile(*ms_GpuProfiler, commandList, "TlasBuilding");

        const benzin::RayTracing_Tlas& tlas = m_RayTracingScene.GetActiveTlas();

        BenzinMakeResourceBarriers(commandList, benzin::TransitionBarrier{ *tlas.GetScratchResource(), benzin::ResourceState::UnorderedAccess });
        commandList.BuildRayTracingAccelerationStructure(tlas);

        commandList.SetComputeSrv(benzin::UnifiedRootParameter::SceneTlas, tlas.GetGpuVirtualAddress());
    }

}
