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
                .max_blocks_per_chunk = 64_kb / sizeof(PipelineState),
                .largest_required_pool_block = sizeof(PipelineState),
            },
        }
    {}

    PipelineStateManager::~PipelineStateManager()
    {
        BenzinWarningIf(
            m_PipelineStates.size() != m_PendingToDestroyPipelineStates.size(),
            "Not all PSOs are released properly. PSO count: {}",
            m_PipelineStates.size() - m_PendingToDestroyPipelineStates.size()
        );

        m_PipelineStates.clear();
    }

    PipelineState* PipelineStateManager::CreatePipelineState(const PipelineStateCreationVariant& creation)
    {
        m_PipelineStates.emplace_back(m_Device, creation);

        return &m_PipelineStates.back();
    }

    void PipelineStateManager::DestroyPipelineState(PipelineState*& pso)
    {
        if (pso == nullptr)
        {
            BenzinWarning("Try to destroy null PSO!");
            return;
        }

        m_PendingToDestroyPipelineStates.push_back(std::exchange(pso, nullptr));
    }

    void PipelineStateManager::DestroyPendingPipelineStates()
    {
        if (m_PendingToDestroyPipelineStates.empty())
        {
            return;
        }

        m_PipelineStates.remove_if([this](const PipelineState& pso)
        {
            return std::ranges::contains(m_PendingToDestroyPipelineStates, &pso);
        });

        m_PendingToDestroyPipelineStates.clear();
    }

    void PipelineStateManager::ReloadPipelineStatesIfNeeded()
    {
        auto& shaderManager = m_Device.GetBackend().GetShaderManager();
        shaderManager.RunIfPendingToReloadShaderIsAvailable([&]
        {
            for (auto& pso : m_PipelineStates)
            {
                bool isPsoNeedsReload = false;
                for (const auto& shader : pso.GetShaders())
                {
                    isPsoNeedsReload |= shaderManager.UpdateShaderState(shader);
                }

                if (isPsoNeedsReload && !pso.Reload())
                {
                    return;
                }
            }
        });
    }

}
