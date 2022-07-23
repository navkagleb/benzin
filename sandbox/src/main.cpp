#include "bootstrap.hpp"

#include <spieler/core/application.hpp>

#if 1

//#include "layers/test_layer.hpp"
#include "layers/tessellation_layer.hpp"

namespace sandbox
{

    class DummyLayer final : public spieler::Layer
    {
    public:
        bool OnAttach() override
        {
            return true;
        }
    };

    class Application : public spieler::Application
    {
    public:
        bool InitExternal() override
        {
            //SPIELER_RETURN_IF_FAILED(m_LayerStack.Push<TestLayer>());
            m_LayerStack.Push<TessellationLayer>();
            
            return true;
        }
    };

} // namespace Sandbox

#else

#include "layers/test_layer.hpp"
//#include "layers/tessellation_layer.hpp"

namespace sandbox
{

    class Application : public spieler::Application
    {
    public:
        bool InitExternal() override
        {
            SPIELER_RETURN_IF_FAILED(m_LayerStack.Push<TestLayer>());
            //SPIELER_RETURN_IF_FAILED(m_LayerStack.Push<TessellationLayer>());

            return true;
        }
    };

} // namespace Sandbox

#endif

spieler::Application* spieler::CreateApplication()
{
    return sandbox::Application::CreateInstance<sandbox::Application>("Sandbox", 1280, 720);
}