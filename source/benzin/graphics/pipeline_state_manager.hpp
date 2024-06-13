#pragma once

#include "benzin/graphics/pipeline_state.hpp"

namespace benzin
{

    class Device;

    class PipelineStateManager
    {
    public:
        explicit PipelineStateManager(Device& device);
        ~PipelineStateManager();

        [[nodiscard]] PipelineState* CreatePipelineState(const PipelineStateCreationVariant& creation);
        void DestroyPipelineState(PipelineState*& pipelineState);

        void ReloadPipelineStatesIfNeeded();

    private:
        Device& m_Device;

        std::pmr::unsynchronized_pool_resource m_PipelineStatePool;
        std::unordered_set<PipelineState*> m_PipelineStates;
    };

}
