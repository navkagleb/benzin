#pragma once

namespace benzin
{

    class CommandLineArgs
    {
    public:
        BenzinDefineNonConstructable(CommandLineArgs);

        static void Initialize(int argc, char** argv);

        static std::filesystem::path g_ExecutableFilePath;

        static uint32_t g_RawLoggerLogOptionFlags;

        static uint32_t g_WindowWidth;
        static uint32_t g_WindowHeight;
        static bool g_IsWindowResizable;

        static bool g_IsPixCapturerEnabled;
        static bool g_IsAdlWrapperEnabled;
        static bool g_IsNvApiWrapperEnabled;

        static uint32_t g_AdapterIndex;
        static std::string_view g_AdapterName;
        static uint32_t g_FrameInFlightCount;
        static GraphicsFormat g_BackBufferFormat;
        static bool g_IsGpuUploadHeapsEnabled;

        static bool g_IsGpuValidationEnabled;
        static bool g_IsSynchronizedCommandQueueValidationEnabled;

        static bool g_IsShaderCacheIgnored;
    };

} // namespace benzin
