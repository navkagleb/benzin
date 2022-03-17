#include <spieler/application.h>

#include "land_layer.h"

namespace Sandbox
{

    class Application : public Spieler::Application
    {
    public:
        bool InitExternal() override
        {
            SPIELER_RETURN_IF_FAILED(PushLayer<LandLayer>(m_Window, m_Renderer) != nullptr);
            
            return true;
        }
    };

} // namespace Sandbox

Spieler::Application* Spieler::CreateApplication()
{
    return new Sandbox::Application;
}