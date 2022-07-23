#include "bootstrap.hpp"

#include <spieler/core/application.hpp>

#include "layers/test_layer.hpp"
#include "layers/tessellation_layer.hpp"

namespace sandbox
{

    class Application : public spieler::Application
    {
    public:
        bool InitExternal() override
        {
            //SPIELER_ASSERT(m_LayerStack.Push<TestLayer>());
            SPIELER_ASSERT(m_LayerStack.Push<TessellationLayer>());
            
            return true;
        }
    };

} // namespace Sandbox

spieler::Application* spieler::CreateApplication()
{
    return sandbox::Application::CreateInstance<sandbox::Application>("Sandbox", 1280, 720);
}