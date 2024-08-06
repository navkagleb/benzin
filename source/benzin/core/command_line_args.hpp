#pragma once

namespace benzin
{

    class CommandLineArgs
    {
    public:
        BenzinDefineNonConstructable(CommandLineArgs);

        static void Initialize(int argc, char** argv);

        static bool GetBool(std::string_view key);
        static uint32_t GetU32(std::string_view key);
        static std::string_view GetString(std::string_view key);
    };

} // namespace benzin
