#pragma once

namespace benzin
{

    class PixCapturer
    {
    public:
        BenzinDefineNonConstructable(PixCapturer);

        static void Initialize();
        static void Shutdown();
    };

}
