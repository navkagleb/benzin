#include "benzin/config/bootstrap.hpp"
#include "benzin/graphics/pipeline_state_manager.hpp"

#include "benzin/core/logger.hpp"
#include "benzin/graphics/backend.hpp"
#include "benzin/graphics/device.hpp"
#include "benzin/graphics/shader_manager.hpp"

namespace benzin
{

    PipelineStateManager::PipelineStateManager(Device& device)
        : m_Device{ device }
        , m_PipelineStatePool
        {
            std::pmr::pool_options
            {
                .max_blocks_per_chunk = 64,
                .largest_required_pool_block = sizeof(PipelineState),
            },
        }
    {}

    PipelineStateManager::~PipelineStateManager()
    {
        BenzinWarningIf(!m_PipelineStates.empty(), "Not all PSOs are released properly. PSO count: {}", m_PipelineStates.size());

        for (auto* pso : m_PipelineStates)
        {
            pso->~PipelineState();
        }
        m_PipelineStates.clear();
    }

    PipelineState* PipelineStateManager::CreatePipelineState(const PipelineStateCreationVariant& creation)
    {
        auto* pipelineState = (PipelineState*)m_PipelineStatePool.allocate(sizeof(PipelineState));
        new(pipelineState) PipelineState{ m_Device, creation };

        m_PipelineStates.insert(pipelineState);

        return pipelineState;
    }

    void PipelineStateManager::DestroyPipelineState(PipelineState*& pipelineState)
    {
        m_PipelineStates.erase(pipelineState);

        pipelineState->~PipelineState();
        m_PipelineStatePool.deallocate(pipelineState, sizeof(PipelineState));

        pipelineState = nullptr;
    }

    void PipelineStateManager::ReloadPipelineStatesIfNeeded()
    {
        auto& shaderManager = m_Device.GetBackend().GetShaderManager();
        shaderManager.RunIfPendingToReloadShaderAvailable([&]
        {
            for (auto* pso : m_PipelineStates)
            {
                bool isPsoNeedsReload = false;

                for (const auto& shader : pso->GetShaders())
                {
                    isPsoNeedsReload |= shaderManager.UpdateShaderState(shader);
                }

                if (isPsoNeedsReload)
                {
                    pso->Reload();
                }
            }
        });
    }

}
