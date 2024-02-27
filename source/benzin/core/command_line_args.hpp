#pragma once

namespace benzin
{

    struct GraphicsDebugLayerParams
    {
        bool IsGPUBasedValidationEnabled = false;
        bool IsSynchronizedCommandQueueValidationEnabled = false;
    };

    class CommandLineArgs
    {
    public:
        BenzinDefineNonConstructable(CommandLineArgs);

        static void Initialize(int argc, char** argv);

        static uint32_t GetWindowWidth();
        static uint32_t GetWindowHeight();
        static bool IsWindowResizable();

        static uint32_t GetAdapterIndex();
        static uint32_t GetFrameInFlightCount();
        static GraphicsFormat GetBackBufferFormat();

        static GraphicsDebugLayerParams GetGraphicsDebugLayerParams();
    };

} // namespace benzin
