#include <spieler/application.hpp>

#include "test_layer.hpp"
#include "land_layer.hpp"

namespace Sandbox
{

    class Application : public Spieler::Application
    {
    public:
        bool InitExternal() override
        {
            SPIELER_RETURN_IF_FAILED(PushLayer<TestLayer>(m_Window, m_Renderer));
            // SPIELER_RETURN_IF_FAILED(PushLayer<LandLayer>(m_Window, m_Renderer));
            
            return true;
        }
    };

} // namespace Sandbox

Spieler::Application* Spieler::CreateApplication()
{
    return new Sandbox::Application;
}