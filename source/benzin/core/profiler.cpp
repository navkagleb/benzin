#include <benzin/config/bootstrap.hpp>
#include <benzin/core/profiler.hpp>

#include <benzin/core/asserter.hpp>
#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    struct ProfilerData
    {
        struct EventInfo
        {
            uint8_t Depth : 7 = 0;
            uint8_t IsParent : 1 = false;
            uint8_t SortIndex = 0;

            union
            {
                std::chrono::high_resolution_clock::time_point BeginTimePoint{};
                std::chrono::microseconds Us;
            };
        };

        static constexpr auto s_MaxEventCount = std::numeric_limits<uint8_t>::max();

        std::unordered_map<std::string_view, EventInfo> EventInfos;
        std::stack<std::string_view, std::vector<std::string_view>> ScopeNames;

        std::vector<ProfileEvent> SortedEvents;

        uint8_t SortCounter = 0;
    };

    static ProfilerData g_Data;

    //

    void Profiler::BeginFrame()
    {
        g_Data.SortCounter = 0;
    }

    void Profiler::EndFrame()
    {
        SortEvents();
    }

    void Profiler::BeginScope(std::string_view name)
    {
        BenzinEnsure(g_Data.EventInfos.size() < ProfilerData::s_MaxEventCount);

        if (!g_Data.ScopeNames.empty())
        {
            auto& parentEventInfo = g_Data.EventInfos[g_Data.ScopeNames.top()];
            parentEventInfo.IsParent = true;
        }

        auto&& [it, _] = g_Data.EventInfos.try_emplace(name, (uint8_t)g_Data.ScopeNames.size());

        auto& eventInfo = (*it).second;
        eventInfo.SortIndex = std::max(eventInfo.SortIndex, g_Data.SortCounter++);
        eventInfo.BeginTimePoint = std::chrono::high_resolution_clock::now();

        g_Data.ScopeNames.push(name);
    }

    void Profiler::EndScope()
    {
        const auto endTimePoint = std::chrono::high_resolution_clock::now();

        auto& eventInfo = g_Data.EventInfos[g_Data.ScopeNames.top()];
        eventInfo.Us = std::chrono::duration_cast<std::chrono::microseconds>(endTimePoint - eventInfo.BeginTimePoint);

        g_Data.ScopeNames.pop();
    }

    void Profiler::SortEvents()
    {
        const bool isNeedResize = g_Data.SortedEvents.size() != g_Data.EventInfos.size();
        if (isNeedResize)
        {
            g_Data.SortedEvents.resize(g_Data.EventInfos.size());
        }

        for (const auto& [name, eventInfo] : g_Data.EventInfos)
        {
            auto& sortedEvent = g_Data.SortedEvents[eventInfo.SortIndex];

            if (isNeedResize)
            {
                sortedEvent.Name = name;
                sortedEvent.Depth = eventInfo.Depth;
                sortedEvent.IsParent = eventInfo.IsParent;
            }

            sortedEvent.Us = eventInfo.Us;
        }
    }

    std::span<const ProfileEvent> Profiler::GetSortedEvents()
    {
        return g_Data.SortedEvents;
    }

}
