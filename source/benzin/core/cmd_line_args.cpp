#include "benzin/config/bootstrap.hpp"
#include "benzin/core/cmd_line_args.hpp"

#include "benzin/core/logger.hpp"

namespace benzin
{

    static void ParseU32(std::string_view cmdLineToParse, void* member)
    {
        auto memberValue = g_Bad32;
        const auto result = std::from_chars(cmdLineToParse.data(), cmdLineToParse.data() + cmdLineToParse.size(), memberValue);

        BenzinAssert(result.ec == std::errc{} && IsGoodUint(memberValue));

        auto& reinterpreMember = *((uint32_t*)member);
        reinterpreMember = memberValue;
    }

    static void ParseStringView(std::string_view cmdLineToParse, void* member)
    {
        *((std::string_view*)member) = cmdLineToParse;
    }

    static void SetTrueIfExists(std::string_view cmdLineToParse, void* member)
    {
        BenzinUnused(cmdLineToParse);

        *((bool*)member) = true;
    }

    static void SetFalseIfExists(std::string_view cmdLineToParse, void* member)
    {
        BenzinUnused(cmdLineToParse);

        *((bool*)member) = false;
    }

    static std::string_view g_ExeFilePath;
    static LogOptionFlags g_LogOptionFlags;

    static uint32_t g_WindowWidth = 1280;
    static uint32_t g_WindowHeight = 720;
    static bool g_IsWindowResizable = true;

    static bool g_IsPixCapturerEnabled = false;
    static bool g_IsAdlWrapperEnabled = true;
    static bool g_IsNvApiWrapperEnabled = true;

    static uint32_t g_AdapterIndex = g_Bad32;
    static std::string_view g_AdapterName;
    static uint32_t g_FrameInFlightCount = 3;
    static GraphicsFormat g_BackBufferFormat = GraphicsFormat::Rgba8Unorm;
    static bool g_IsGpuUploadHeapsEnabled = true;

    static bool g_IsGpuValidationEnabled = true;
    static bool g_IsSynchronizedCommandQueueValidationEnabled = true;

    static bool g_IsShaderCacheIgnored = false;

    //

    void CmdLineArgs::Initialize(int argc, char** argv)
    {
        struct SupportedArg
        {
            using ParseCallbackNoArgs = void (*)();
            using ParseCallback = void (*)(std::string_view cmdLineToParse, void* member);

            std::string_view Name;
            void* Member = nullptr;
            std::variant<ParseCallbackNoArgs, ParseCallback> CallbackVariant;

            void ParseIfMathes(std::string_view currentCmdLine) const
            {
                if (!currentCmdLine.starts_with(Name))
                {
                    return;
                }

                CallbackVariant | MakeVisitorMatch(
                    [](ParseCallbackNoArgs callback) { callback(); },
                    [&](ParseCallback callback) { callback(currentCmdLine.substr(Name.size()), Member); }
                );
            }
        };

        const auto supportedArgs = std::to_array<SupportedArg>(
        {
            { "-log_time", nullptr, [] { g_LogOptionFlags.Set(LogOptionFlag::Time); }},
            { "-log_thread_id", nullptr, [] { g_LogOptionFlags.Set(LogOptionFlag::ThreadId); } },
            { "-log_file_name", nullptr, [] { g_LogOptionFlags.Set(LogOptionFlag::FileName); } },

            { "-window_width:", &g_WindowWidth, ParseU32 },
            { "-window_height:", &g_WindowHeight, ParseU32 },
            { "-disable_window_resizing", &g_IsWindowResizable, SetFalseIfExists },

            { "-pix", &g_IsPixCapturerEnabled, SetTrueIfExists },
            { "-no_adl_wrapper", &g_IsAdlWrapperEnabled, SetFalseIfExists },
            { "-no_nvapi_wrapper", &g_IsNvApiWrapperEnabled, SetFalseIfExists },

            { "-adapter_index:", &g_AdapterIndex, ParseU32 },
            { "-adapter_name:", &g_AdapterName, ParseStringView },
            { "-frame_in_flight_count:", &g_FrameInFlightCount, ParseU32 },
            { "-no_gpu_upload_heaps", &g_IsGpuUploadHeapsEnabled, SetFalseIfExists },

            { "-no_gpu_based_validation", &g_IsGpuValidationEnabled, SetFalseIfExists },
            { "-no_sync_command_queue_validation", &g_IsSynchronizedCommandQueueValidationEnabled, SetFalseIfExists },

            { "-ignore_shader_cache", &g_IsShaderCacheIgnored, SetTrueIfExists },
        });

        g_ExeFilePath = argv[0];

        for (const int i : std::views::iota(0, argc))
        {
            const std::string_view currentArg = argv[i];
            BenzinTrace("CommandLineArg {}: {}", i, currentArg);

            if (!currentArg.starts_with('-'))
            {
                continue;
            }

            for (const auto& supportedArg : supportedArgs)
            {
                supportedArg.ParseIfMathes(currentArg);
            }
        }
    }

#define BenzinImplCmdLineArg(functionName, value) \
    decltype(value) CmdLineArgs::functionName() { return value; }

    BenzinImplCmdLineArg(GetRawLogOptionFlags, g_LogOptionFlags.GetRawBits())
    BenzinImplCmdLineArg(GetWindowWidth, g_WindowWidth)
    BenzinImplCmdLineArg(GetWindowHeight, g_WindowHeight)
    BenzinImplCmdLineArg(IsWindowResizable, g_IsWindowResizable)
    BenzinImplCmdLineArg(IsPixCapturerEnabled, g_IsPixCapturerEnabled)
    BenzinImplCmdLineArg(IsAdlWrapperEnabled, g_IsAdlWrapperEnabled)
    BenzinImplCmdLineArg(IsNvApiWrapperEnabled, g_IsNvApiWrapperEnabled)
    BenzinImplCmdLineArg(GetAdapterIndex, g_AdapterIndex)
    BenzinImplCmdLineArg(GetAdapterName, g_AdapterName)
    BenzinImplCmdLineArg(GetFrameInFlightCount, g_FrameInFlightCount)
    BenzinImplCmdLineArg(GetBackBufferFormat, g_BackBufferFormat)
    BenzinImplCmdLineArg(IsGpuUploadHeapsEnabled, g_IsGpuUploadHeapsEnabled)
    BenzinImplCmdLineArg(IsGpuValidationEnabled, g_IsGpuValidationEnabled)
    BenzinImplCmdLineArg(IsSynchronizedCommandQueueValidationEnabled, g_IsSynchronizedCommandQueueValidationEnabled)
    BenzinImplCmdLineArg(IsShaderCacheIgnored, g_IsShaderCacheIgnored)

#undef BenzinImplCmdLineArg

}
