#pragma once

namespace benzin
{

    struct ProfileEvent
    {
        std::string_view Name;
        std::chrono::microseconds Us = std::chrono::microseconds::zero();
        uint8_t Depth : 7 = 0;
        uint8_t IsParent : 1 = false;
    };

    class Profiler
    {
    public:
        static void BeginFrame();
        static void EndFrame();

        static void BeginScope(std::string_view name);
        static void EndScope();

        static std::span<const ProfileEvent> GetSortedEvents();

    private:
        static void SortEvents();
    };

}

#define BenzinProfile() \
    benzin::Profiler::BeginScope(__FUNCTION__); \
    BenzinExecuteOnScopeExit([] { benzin::Profiler::EndScope(); })

#define BenzinScopeProfile(name) \
    benzin::Profiler::BeginScope(name); \
    BenzinExecuteOnScopeExit([] { benzin::Profiler::EndScope(); })
