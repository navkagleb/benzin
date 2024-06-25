#pragma once

namespace benzin
{

    struct GraphicsDebugLayerParams
    {
        bool IsGpuBasedValidationEnabled = true;
        bool IsSynchronizedCommandQueueValidationEnabled = true;
    };

    class CommandLineArgs
    {
    public:
        BenzinDefineNonConstructable(CommandLineArgs);

        static void Initialize(int argc, char** argv);

        static inline std::filesystem::path g_ExecutableFilePath;

        static inline uint32_t g_WindowWidth = 1280;
        static inline uint32_t g_WindowHeight = 720;
        static inline bool g_IsWindowResizable = true;

        static inline bool g_IsAdlWrapperEnabled = true;
        static inline bool g_IsNvApiWrapperEnabled = true;

        static inline uint32_t g_AdapterIndex = 0;
        static inline uint32_t g_FrameInFlightCount = 3;
        static inline GraphicsFormat g_BackBufferFormat = GraphicsFormat::Rgba8Unorm;
        static inline bool g_IsGpuUploadHeapsEnabled = true;

        static inline GraphicsDebugLayerParams g_GraphicsDebugLayerParams;

        static inline bool g_IsShaderCacheIgnored = false;
    };

} // namespace benzin
