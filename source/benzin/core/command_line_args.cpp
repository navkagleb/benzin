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

        template <typename T> requires std::is_arithmetic_v<T>
        void ParseArithmeticOptional(std::string_view commandLineToParse, void* member)
        {
            // #TODO: Duplication with 'ParseArithmetic'

            BenzinAssert(member);

            auto memberValue = std::numeric_limits<T>::max();
            const auto result = std::from_chars(commandLineToParse.data(), commandLineToParse.data() + commandLineToParse.size(), memberValue);

            if (result.ec == std::errc{})
            {
                BenzinAssert(memberValue != std::numeric_limits<T>::max());

                auto& reinterpreMember = *reinterpret_cast<std::optional<T>*>(member);
                reinterpreMember = memberValue;
            }
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
            SupportedCommandLineArg{ "-window_width:", &m_WindowWidth, ParseArithmetic<uint32_t> },
            SupportedCommandLineArg{ "-window_height:", &m_WindowHeight, ParseArithmetic<uint32_t> },
            SupportedCommandLineArg{ "-is_window_resizable:", &m_IsWindowResizable, ParseArithmetic<uint32_t> },

            SupportedCommandLineArg{ "-adapter_index:", &m_AdapterIndex, ParseArithmeticOptional<uint32_t> },
            SupportedCommandLineArg{ "-frame_in_flight_count:", &m_FrameInFlightCount, ParseArithmeticOptional<uint32_t> },
            SupportedCommandLineArg{ "-is_gpu_based_validation_enabled:", &m_IsEnabledGPUBasedValidation, ParseArithmetic<uint32_t> },
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
