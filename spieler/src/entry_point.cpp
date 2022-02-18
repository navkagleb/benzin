#include "application.h"

int __stdcall WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd)
{
    Spieler::Application application;
    
    if (!application.Init("Spieler", 1280, 720))
    {
        application.Shutdown();
        return -1;
    }

    return application.Run();
}