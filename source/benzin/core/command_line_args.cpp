#include "benzin/config/bootstrap.hpp"
#include "benzin/core/command_line_args.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    using ParseCallbackNoArgs = void (*)();
    using ParseCallback = void (*)(std::string_view commandLineToParse, void* member);

    template <typename T> requires std::is_arithmetic_v<T>
    static void ParseArithmetic(std::string_view commandLineToParse, void* member)
    {
        auto memberValue = std::numeric_limits<T>::max();
        const auto result = std::from_chars(commandLineToParse.data(), commandLineToParse.data() + commandLineToParse.size(), memberValue);

        BenzinAssert(result.ec == std::errc{} && memberValue != std::numeric_limits<T>::max());

        auto& reinterpreMember = *((T*)member);
        reinterpreMember = memberValue;
    }

    static void ParseStringView(std::string_view commandLineToParse, void* member)
    {
        *((std::string_view*)member) = commandLineToParse;
    }

    static void SetTrueIfExists(std::string_view commandLineToParse, void* member)
    {
        BenzinUnused(commandLineToParse);

        *((bool*)member) = true;
    }

    static void SetFalseIfExists(std::string_view commandLineToParse, void* member)
    {
        BenzinUnused(commandLineToParse);

        *((bool*)member) = false;
    }

    struct SupportedCommandLineArg
    {
        std::string_view Name;
        void* Member = nullptr;
        std::variant<ParseCallbackNoArgs, ParseCallback> CallbackVariant;

        void ParseIfMathes(std::string_view currentCommandLine) const
        {
            if (!currentCommandLine.starts_with(Name))
            {
                return;
            }

            CallbackVariant | MakeVisitorMatch(
                [](ParseCallbackNoArgs callback) { callback(); },
                [&](ParseCallback callback) { callback(currentCommandLine.substr(Name.size()), Member); }
            );
        }
    };

    //

    std::filesystem::path CommandLineArgs::g_ExecutableFilePath;

    uint32_t CommandLineArgs::g_RawLoggerLogOptionFlags = 0;

    uint32_t CommandLineArgs::g_WindowWidth = 1280;
    uint32_t CommandLineArgs::g_WindowHeight = 720;
    bool CommandLineArgs::g_IsWindowResizable = true;

    bool CommandLineArgs::g_IsPixCapturerEnabled = false;
    bool CommandLineArgs::g_IsAdlWrapperEnabled = true;
    bool CommandLineArgs::g_IsNvApiWrapperEnabled = true;

    uint32_t CommandLineArgs::g_AdapterIndex = g_InvalidUnsigned<uint32_t>;
    std::string_view CommandLineArgs::g_AdapterName;
    uint32_t CommandLineArgs::g_FrameInFlightCount = 3;
    GraphicsFormat CommandLineArgs::g_BackBufferFormat = GraphicsFormat::Rgba8Unorm;
    bool CommandLineArgs::g_IsGpuUploadHeapsEnabled = true;

    bool CommandLineArgs::g_IsGpuValidationEnabled = true;
    bool CommandLineArgs::g_IsSynchronizedCommandQueueValidationEnabled = true;

    bool CommandLineArgs::g_IsShaderCacheIgnored = false;

    void CommandLineArgs::Initialize(int argc, char** argv)
    {
        const auto supportedArgs = std::to_array(
        {
            SupportedCommandLineArg{ "-log_time", nullptr, [] { CommandLineArgs::g_RawLoggerLogOptionFlags |= (uint32_t)LogOptionFlag::Time; } },
            SupportedCommandLineArg{ "-log_thread_id", nullptr, [] { CommandLineArgs::g_RawLoggerLogOptionFlags |= (uint32_t)LogOptionFlag::ThreadId; } },
            SupportedCommandLineArg{ "-log_file_name", nullptr, [] { CommandLineArgs::g_RawLoggerLogOptionFlags |= (uint32_t)LogOptionFlag::FileName; } },

            SupportedCommandLineArg{ "-window_width:", &g_WindowWidth, ParseArithmetic<decltype(g_WindowWidth)> },
            SupportedCommandLineArg{ "-window_height:", &g_WindowHeight, ParseArithmetic<decltype(g_WindowHeight)> },
            SupportedCommandLineArg{ "-disable_window_resizing", &g_IsWindowResizable, SetFalseIfExists },

            SupportedCommandLineArg{ "-pix", &g_IsPixCapturerEnabled, SetTrueIfExists },
            SupportedCommandLineArg{ "-no_adl_wrapper", &g_IsAdlWrapperEnabled, SetFalseIfExists },
            SupportedCommandLineArg{ "-no_nvapi_wrapper", &g_IsNvApiWrapperEnabled, SetFalseIfExists },

            SupportedCommandLineArg{ "-adapter_index:", &g_AdapterIndex, ParseArithmetic<decltype(g_AdapterIndex)> },
            SupportedCommandLineArg{ "-adapter_name:", &g_AdapterName, ParseStringView },
            SupportedCommandLineArg{ "-frame_in_flight_count:", &g_FrameInFlightCount, ParseArithmetic<decltype(g_FrameInFlightCount)> },
            SupportedCommandLineArg{ "-no_gpu_upload_heaps", &g_IsGpuUploadHeapsEnabled, SetFalseIfExists },

            SupportedCommandLineArg{ "-no_gpu_based_validation", &g_IsGpuValidationEnabled, SetFalseIfExists },
            SupportedCommandLineArg{ "-no_sync_command_queue_validation", &g_IsSynchronizedCommandQueueValidationEnabled, SetFalseIfExists },

            SupportedCommandLineArg{ "-ignore_shader_cache", &g_IsShaderCacheIgnored, SetTrueIfExists },
        });

        g_ExecutableFilePath = argv[0];

        BenzinTrace("ExecutablePath: {}", g_ExecutableFilePath.string());

        for (const int i : std::views::iota(1, argc))
        {
            const std::string_view currentArg = argv[i];

            if (!currentArg.starts_with('-'))
            {
                continue;
            }

            for (const auto& supportedArg : supportedArgs)
            {
                supportedArg.ParseIfMathes(currentArg);
            }

            BenzinTrace("CommandLineArg {}: {}", i, currentArg);
        }
    }

} // namespace benzin
