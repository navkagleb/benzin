#include "application.hpp"

int __stdcall WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd)
{
    Spieler::Application* application = Spieler::CreateApplication();
    
    if (!application->InitInternal("Spieler", 1280, 720))
    {
        delete application;
        return -1;
    }

    const int result = application->Run();

    delete application;
    return result;
}