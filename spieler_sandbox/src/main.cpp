#include "bootstrap.hpp"

#include <spieler/core/application.hpp>

#include "test_layer.hpp"
#include "land_layer.hpp"

namespace sandbox
{

    class Application : public spieler::Application
    {
    public:
        bool InitExternal() override
        {
            SPIELER_RETURN_IF_FAILED(PushLayer<TestLayer>(*m_Window));
            //SPIELER_RETURN_IF_FAILED(PushLayer<LandLayer>(m_Window, m_Renderer));
            
            return true;
        }
    };

} // namespace Sandbox

spieler::Application* spieler::CreateApplication()
{
    return sandbox::Application::CreateInstance<sandbox::Application>("Sandbox", 1280, 720);
}