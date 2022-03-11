#include "application.h"

int __stdcall WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd)
{
    auto& application = Spieler::Application::GetInstance();
    
    const bool status = application.Init("Spieler", 1280, 720);

    if (!status)
    {
        return -1;
    }

    return application.Run();
}