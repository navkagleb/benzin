#pragma once

#include <benzin/engine/render_pass.hpp>

#include <shaders/joint/constant_buffer_types.hpp>

namespace benzin
{

    template <typename>
    class ConstantBuffer;

    class Scene;

}

namespace sandbox
{

    class FullScreenDebugPass : public benzin::RenderPass
    {
    public:
        FullScreenDebugPass();
        ~FullScreenDebugPass() override;

        bool IsDependentOnViewport() const override { return true; }

        void OnUpdate() override;
        void OnRender() const override;

    private:
        using PassConstantBuffer = benzin::ConstantBuffer<joint::FullScreenDebugConstants>;

        std::unique_ptr<PassConstantBuffer> m_PassConstantBuffer;
    };

}
