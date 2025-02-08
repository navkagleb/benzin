#pragma once

namespace benzin
{

    struct ProfileEvent
    {
        const char* Name = nullptr;
        std::chrono::microseconds Us = std::chrono::microseconds::zero();
        uint8_t Depth : 7 = 0;
        uint8_t IsParent : 1 = false;
    };

    class Profiler
    {
    public:
        friend class ScopedProfileEvent;

        static void Initialize();

        static void BeginFrame();
        static void EndFrame();

        static std::span<const ProfileEvent> GetSortedEvents();

    private:
        static void BeginScope(std::string_view name);
        static void EndScope();

        static void SortEvents();
    };

    class ScopedProfileEvent
    {
    public:
        explicit ScopedProfileEvent(std::string_view name);
        ~ScopedProfileEvent();
    };

}

#define BenzinScopeProfile(name) const benzin::ScopedProfileEvent BenzinUniqueVariableName(_scopedProfilerEvent){ name }
#define BenzinProfile() BenzinScopeProfile(__FUNCTION__)
