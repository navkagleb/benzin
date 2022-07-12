#include "spieler/config/bootstrap.hpp"

#include "spieler/core/application.hpp"

#if defined(SPIELER_PLATFORM_WINDOWS)

namespace spieler
{

    int Main(int argc, char** argv)
    {
        spieler::Application* application{ spieler::CreateApplication() };

        if (!application)
        {
            return -1;
        }

        application->Run();

        delete application;

        return 0;
    }

} // namespace spieler

#if defined(SPIELER_FINAL)

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
    return spieler::Main(__argc, __argv);
}

#else

int main(int argc, char** argv)
{
    return spieler::Main(argc, argv);
}

#endif // defined(SPIELER_FINAL)

#endif // defined(SPIELER_WIN64)