#pragma once

namespace benzin
{

    struct ProfileNode
    {
        using Clock = std::chrono::high_resolution_clock; // TODO: Replace with QueryPerformanceCounter
        using TimePoint = Clock::time_point;
        using Duration = Clock::duration;

        std::string_view m_Name;
        mutable std::vector<std::unique_ptr<ProfileNode>> m_Children;
        std::unordered_map<std::string_view, ProfileNode*> m_ChildrenMap;
        ProfileNode* m_Parent = nullptr;

        uint32_t m_HitCount = 0;
        uint32_t m_ImGuiHitCount = 0;

        Duration m_Duration = {};
        Duration m_AccumulatedDuration = {};
        Duration m_ImGuiDuration = {};

        uint32_t m_CurrentChildOffset = 0;
        uint32_t m_SortIndex = 0;

        mutable bool m_IsSortingNeeded = false;

        void SortChildren() const;
    };

    class Profiler
    {
    public:
        static void BeginFrame();
        static void EndFrame();

        static void ResetAccumulatedData(uint32_t frameCount);

        static const ProfileNode& GetRootNode();
    };

    class ScopedProfileEvent
    {
    public:
        explicit ScopedProfileEvent(std::string_view name);
        ~ScopedProfileEvent();

    private:
        ProfileNode* m_Node;
    };

}

#define BenzinScopeProfile(name) const benzin::ScopedProfileEvent BenzinUniqueVariableName(_scopedProfilerEvent){ name }
#define BenzinProfile() BenzinScopeProfile(__FUNCTION__)
