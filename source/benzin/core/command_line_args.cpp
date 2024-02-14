#include "benzin/config/bootstrap.hpp"
#include "benzin/core/command_line_args.hpp"

namespace benzin
{

    namespace
    {

        using ParseCallback = void (*)(std::string_view commandLineToParse, void* member);

        template <typename T> requires std::is_arithmetic_v<T>
        void ParseArithmetic(std::string_view commandLineToParse, void* member)
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

        template <>
        void ParseArithmetic<bool>(std::string_view commandLineToParse, void* member)
        {
            return ParseArithmetic<uint32_t>(commandLineToParse, member);
        }

        void ParseBool(std::string_view commandLineToParse, void* member)
        {
            BenzinAssert(member);

            if (commandLineToParse.empty())
            {
                auto& reinterpreMember = *reinterpret_cast<bool*>(member);
                reinterpreMember = true;
            }
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

    } // anonymous namespace

    CommandLineArgs::CommandLineArgs(int argc, char** argv)
    {
        const auto supportedArgs = std::to_array(
        {
            SupportedCommandLineArg{ "-window_width:", &m_WindowWidth, ParseArithmetic<decltype(m_WindowWidth)> },
            SupportedCommandLineArg{ "-window_height:", &m_WindowHeight, ParseArithmetic<decltype(m_WindowHeight)> },
            SupportedCommandLineArg{ "-is_window_resizable:", &m_IsWindowResizable, ParseArithmetic<decltype(m_IsWindowResizable)> },

            SupportedCommandLineArg{ "-adapter_index:", &m_AdapterIndex, ParseArithmetic<decltype(m_AdapterIndex)> },
            SupportedCommandLineArg{ "-frame_in_flight_count:", &m_FrameInFlightCount, ParseArithmetic<decltype(m_FrameInFlightCount)> },
            SupportedCommandLineArg{ "-is_gpu_based_validation_enabled:", &m_IsEnabledGPUBasedValidation, ParseArithmetic<decltype(m_IsEnabledGPUBasedValidation)> },
        });

        m_ExecutablePath = argv[0];

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

} // namespace benzin
