#include "benzin/config/bootstrap.hpp"
#include "benzin/core/command_line_args.hpp"

#include "benzin/core/asserter.hpp"
#include "benzin/core/logger.hpp"

namespace benzin
{

    static void ParseU32(std::string_view commandLineToParse, void* member)
    {
        auto memberValue = g_InvalidUnsigned<uint32_t>;
        const auto result = std::from_chars(commandLineToParse.data(), commandLineToParse.data() + commandLineToParse.size(), memberValue);

        BenzinAssert(result.ec == std::errc{} && IsValidUnsigned(memberValue));

        auto& reinterpreMember = *((uint32_t*)member);
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

    static std::unordered_map<std::string_view, bool> g_BoolMap;
    static std::unordered_map<std::string_view, uint32_t> g_U32Map;
    static std::unordered_map<std::string_view, std::string_view> g_StringMap;

    template <typename T>
    static T* GetPtrFromMap(std::unordered_map<std::string_view, T>& map, std::string_view key)
    {
        BenzinAssert(map.contains(key));
        return &map[key];
    }

    template <typename T>
    static T GetFromMap(std::unordered_map<std::string_view, T>& map, std::string_view key)
    {
        return *GetPtrFromMap<T>(map, key);
    }

    static void SetDefaultValues()
    {
        g_U32Map["RawLoggerLogOptionFlags"] = 0;

        g_U32Map["WindowWidth"] = 1280;
        g_U32Map["WindowHeight"] = 720;
        g_BoolMap["IsWindowResizable"] = true;

        g_BoolMap["IsPixCapturerEnabled"] = false;
        g_BoolMap["IsAdlWrapperEnabled"] = true;
        g_BoolMap["IsNvApiWrapperEnabled"] = true;

        g_U32Map["AdapterIndex"] = g_InvalidUnsigned<uint32_t>;
        g_StringMap["AdapterName"] = "";
        g_U32Map["FrameInFlightCount"] = 3;
        g_U32Map["BackBufferFormat"] = (uint32_t)GraphicsFormat::Rgba8Unorm;
        g_BoolMap["IsGpuUploadHeapsEnabled"] = true;

        g_BoolMap["IsGpuValidationEnabled"] = true;
        g_BoolMap["IsSynchronizedCommandQueueValidationEnabled"] = true;

        g_BoolMap["IsShaderCacheIgnored"] = false;
    }

    static void ParseCommandLineArgs(int argc, char** argv)
    {
        struct SupportedArg
        {
            using ParseCallbackNoArgs = void (*)();
            using ParseCallback = void (*)(std::string_view commandLineToParse, void* member);

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

        const auto supportedArgs = std::to_array<SupportedArg>(
        {
            { "-log_time", nullptr, [] { *GetPtrFromMap(g_U32Map, "RawLoggerLogOptionFlags") |= (uint32_t)LogOptionFlag::Time; }},
            { "-log_thread_id", nullptr, [] { *GetPtrFromMap(g_U32Map, "RawLoggerLogOptionFlags") |= (uint32_t)LogOptionFlag::ThreadId; } },
            { "-log_file_name", nullptr, [] { *GetPtrFromMap(g_U32Map, "RawLoggerLogOptionFlags") |= (uint32_t)LogOptionFlag::FileName; } },

            { "-window_width:", GetPtrFromMap(g_U32Map, "WindowWidth"), ParseU32 },
            { "-window_height:", GetPtrFromMap(g_U32Map, "WindowHeight"), ParseU32 },
            { "-disable_window_resizing", GetPtrFromMap(g_BoolMap, "IsWindowResizable"), SetFalseIfExists },

            { "-pix", GetPtrFromMap(g_BoolMap, "IsPixCapturerEnabled"), SetTrueIfExists },
            { "-no_adl_wrapper", GetPtrFromMap(g_BoolMap, "IsAdlWrapperEnabled"), SetFalseIfExists },
            { "-no_nvapi_wrapper", GetPtrFromMap(g_BoolMap, "IsNvApiWrapperEnabled"), SetFalseIfExists },

            { "-adapter_index:", GetPtrFromMap(g_U32Map, "AdapterIndex"), ParseU32 },
            { "-adapter_name:", GetPtrFromMap(g_StringMap, "AdapterName"), ParseStringView },
            { "-frame_in_flight_count:", GetPtrFromMap(g_U32Map, "FrameInFlightCount"), ParseU32 },
            { "-no_gpu_upload_heaps", GetPtrFromMap(g_BoolMap, "IsGpuUploadHeapsEnabled"), SetFalseIfExists },

            { "-no_gpu_based_validation", GetPtrFromMap(g_BoolMap, "IsGpuValidationEnabled"), SetFalseIfExists },
            { "-no_sync_command_queue_validation", GetPtrFromMap(g_BoolMap, "IsSynchronizedCommandQueueValidationEnabled"), SetFalseIfExists },

            { "-ignore_shader_cache", GetPtrFromMap(g_BoolMap, "IsShaderCacheIgnored"), SetTrueIfExists },
        });

        g_StringMap["ExecutableFilePath"] = argv[0];

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

    //

    void CommandLineArgs::Initialize(int argc, char** argv)
    {
        SetDefaultValues();
        ParseCommandLineArgs(argc, argv);
    }

    bool CommandLineArgs::GetBool(std::string_view key)
    {
        return GetFromMap(g_BoolMap, key);
    }

    uint32_t CommandLineArgs::GetU32(std::string_view key)
    {
        return GetFromMap(g_U32Map, key);
    }

    std::string_view CommandLineArgs::GetString(std::string_view key)
    {
        return GetFromMap(g_StringMap, key);
    }

} // namespace benzin
