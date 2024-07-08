#include "benzin/config/bootstrap.hpp"
#include "benzin/core/command_line_args.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    using ParseCallback = void (*)(std::string_view commandLineToParse, void* member);

    template <typename T> requires std::is_arithmetic_v<T>
    static void ParseArithmetic(std::string_view commandLineToParse, void* member)
    {
        BenzinAssert(member);

        auto memberValue = std::numeric_limits<T>::max();
        const auto result = std::from_chars(commandLineToParse.data(), commandLineToParse.data() + commandLineToParse.size(), memberValue);

        BenzinAssert(result.ec == std::errc{} && memberValue != std::numeric_limits<T>::max());

        auto& reinterpreMember = *((T*)member);
        reinterpreMember = memberValue;
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
        ParseCallback Callback = nullptr;

        void ParseIfMathes(std::string_view currentCommandLine) const
        {
            if (currentCommandLine.starts_with(Name))
            {
                Callback(currentCommandLine.substr(Name.size()), Member);
            }
        }
    };

    //

    void CommandLineArgs::Initialize(int argc, char** argv)
    {
        const auto supportedArgs = std::to_array(
        {
            SupportedCommandLineArg{ "-window_width:", &g_WindowWidth, ParseArithmetic<decltype(g_WindowWidth)> },
            SupportedCommandLineArg{ "-window_height:", &g_WindowHeight, ParseArithmetic<decltype(g_WindowHeight)> },
            SupportedCommandLineArg{ "-disable_window_resizing", &g_IsWindowResizable, SetFalseIfExists },

            SupportedCommandLineArg{ "-no_adl_wrapper", &g_IsAdlWrapperEnabled, SetFalseIfExists },
            SupportedCommandLineArg{ "-no_nvapi_wrapper", &g_IsNvApiWrapperEnabled, SetFalseIfExists },

            SupportedCommandLineArg{ "-adapter_index:", &g_AdapterIndex, ParseArithmetic<decltype(g_AdapterIndex)> },
            SupportedCommandLineArg{ "-frame_in_flight_count:", &g_FrameInFlightCount, ParseArithmetic<decltype(g_FrameInFlightCount)> },
            SupportedCommandLineArg{ "-no_gpu_upload_heaps", &g_IsGpuUploadHeapsEnabled, SetFalseIfExists },

            SupportedCommandLineArg{ "-no_gpu_based_validation", &g_GraphicsDebugLayerParams.IsGpuBasedValidationEnabled, SetFalseIfExists },
            SupportedCommandLineArg{ "-no_sync_command_queue_validation", &g_GraphicsDebugLayerParams.IsSynchronizedCommandQueueValidationEnabled, SetFalseIfExists },

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
