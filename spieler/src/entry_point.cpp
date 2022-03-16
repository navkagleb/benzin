#include "application.h"

int __stdcall WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd)
{
    auto& application = *Spieler::CreateApplication();
    
    if (!application.InitInternal("Spieler", 1280, 720))
    {
        return -1;
    }

    return application.Run();
}