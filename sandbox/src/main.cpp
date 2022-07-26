#include "bootstrap.hpp"

#include <spieler/core/application.hpp>

#include "layers/test_layer.hpp"
#include "layers/tessellation_layer.hpp"
#include "layers/dynamic_indexing_layer.hpp"

namespace sandbox
{

    class Application : public spieler::Application
    {
    public:
        Application(const spieler::Application::Config& config)
            : spieler::Application{ config }
        {
            //SPIELER_ASSERT(m_LayerStack.Push<TestLayer>());
            //SPIELER_ASSERT(m_LayerStack.Push<TessellationLayer>());
            SPIELER_ASSERT(m_LayerStack.Push<DynamicIndexingLayer>());
        }
    };

} // namespace sandbox

spieler::Application* spieler::CreateApplication()
{
    const spieler::Application::Config config
    {
        .Title{ "Sandbox" },
        .Width{ 1280 },
        .Height{ 720 }
    };

    return new sandbox::Application{ config };
}