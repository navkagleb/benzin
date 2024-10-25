#pragma once

#include <benzin/engine/render_pass.hpp>

namespace benzin
{

    class PipelineState;

}

namespace sandbox
{

    class ReferenceDenoiserPass : public benzin::RenderPass
    {
    public:
        ReferenceDenoiserPass();
        ~ReferenceDenoiserPass();

    private:
        benzin::PipelineState* m_Pso = nullptr;
    };

}
