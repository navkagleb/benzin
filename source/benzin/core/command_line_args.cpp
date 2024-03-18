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

        if (result.ec == std::errc{})
        {
            BenzinAssert(memberValue != std::numeric_limits<T>::max());

            auto& reinterpreMember = *reinterpret_cast<T*>(member);
            reinterpreMember = memberValue;
        }
    }

    static void SetFalseIfExists([[maybe_unused]] std::string_view commandLineToParse, void* member)
    {
        *reinterpret_cast<bool*>(member) = false;
    }

    struct SupportedCommandLineArg
    {
        std::string_view Name;
        void* Member = nullptr;
        ParseCallback Callback = nullptr;

        void InvokeIfMatches(std::string_view currentCommandLine) const
        {
            if (currentCommandLine.starts_with(Name))
            {
                Callback(currentCommandLine.substr(Name.size()), Member);
            }
        }
    };

    struct CommandLineArgsState
    {
        std::filesystem::path ExecutablePath;

        uint32_t WindowWidth = 1280;
        uint32_t WindowHeight = 720;
        bool IsWindowResizable = true;

        uint32_t AdapterIndex = 0;
        uint32_t FrameInFlightCount = 3;
        GraphicsFormat BackBufferFormat = GraphicsFormat::Rgba8Unorm;
        bool IsGpuUploadHeapsEnabled = true;

        GraphicsDebugLayerParams GraphicsDebugLayerParams;

        CommandLineArgsState(int argc, char** argv)
        {
            const auto supportedArgs = std::to_array(
            {
                SupportedCommandLineArg{ "-window_width:", &WindowWidth, ParseArithmetic<decltype(WindowWidth)> },
                SupportedCommandLineArg{ "-window_height:", &WindowHeight, ParseArithmetic<decltype(WindowHeight)> },
                SupportedCommandLineArg{ "-disable_window_resizing", &IsWindowResizable, SetFalseIfExists },

                SupportedCommandLineArg{ "-adapter_index:", &AdapterIndex, ParseArithmetic<decltype(AdapterIndex)> },
                SupportedCommandLineArg{ "-frame_in_flight_count:", &FrameInFlightCount, ParseArithmetic<decltype(FrameInFlightCount)> },
                SupportedCommandLineArg{ "-force_disable_gpu_upload_heaps", &IsGpuUploadHeapsEnabled, SetFalseIfExists },

                SupportedCommandLineArg{ "-force_disable_gpu_based_validation", &GraphicsDebugLayerParams.IsGpuBasedValidationEnabled, SetFalseIfExists },
                SupportedCommandLineArg{ "-force_disable_sync_command_queue_validation", &GraphicsDebugLayerParams.IsSynchronizedCommandQueueValidationEnabled, SetFalseIfExists },
            });

            ExecutablePath = argv[0];

            for (int i = 1; i < argc; ++i)
            {
                const std::string_view currentArg = argv[i];

                if (!currentArg.starts_with('-'))
                {
                    continue;
                }

                for (const auto& supportedArg : supportedArgs)
                {
                    supportedArg.InvokeIfMatches(currentArg);
                }

                BenzinTrace("CommandLineArg {}: {}", i, currentArg);
            }
        }
    };

    static std::unique_ptr<CommandLineArgsState> g_CommandLineArgsState;

    //

    void CommandLineArgs::Initialize(int argc, char** argv)
    {
        MakeUniquePtr(g_CommandLineArgsState, argc, argv);
    }

    uint32_t CommandLineArgs::GetWindowWidth() { return g_CommandLineArgsState->WindowWidth; }
    uint32_t CommandLineArgs::GetWindowHeight() { return g_CommandLineArgsState->WindowHeight; }
    bool CommandLineArgs::IsWindowResizable() { return g_CommandLineArgsState->IsWindowResizable; }
    uint32_t CommandLineArgs::GetAdapterIndex() { return g_CommandLineArgsState->AdapterIndex; }
    uint32_t CommandLineArgs::GetFrameInFlightCount() { return g_CommandLineArgsState->FrameInFlightCount; }
    GraphicsFormat CommandLineArgs::GetBackBufferFormat() { return g_CommandLineArgsState->BackBufferFormat; }
    bool CommandLineArgs::IsGpuUploadHeapsEnabled() { return g_CommandLineArgsState->IsGpuUploadHeapsEnabled; }
    GraphicsDebugLayerParams CommandLineArgs::GetGraphicsDebugLayerParams() { return g_CommandLineArgsState->GraphicsDebugLayerParams; }

} // namespace benzin
