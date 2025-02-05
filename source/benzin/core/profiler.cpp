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
            const char* Name = nullptr;

            uint8_t Depth : 6 = 0;
            uint8_t IsParent : 1 = false;
            uint8_t IsProcessed : 1 = false;
            uint8_t SortIndex = 0;

            std::chrono::high_resolution_clock::time_point BeginTimePoint{};
            std::chrono::microseconds Us = std::chrono::microseconds::zero();
        };

        static constexpr auto s_MaxEventCount = std::numeric_limits<uint8_t>::max();

        std::unordered_map<uint64_t, EventInfo> EventInfos;
        std::unordered_map<uint8_t, uint64_t> SortedEventHashes;
        std::stack<uint64_t, std::vector<uint64_t>> EventHashStack;

        std::vector<ProfileEvent> SortedEvents;

        uint8_t SortCounter = 0;
    };

    static ProfilerData g_Data;

    static uint64_t CalcEventHash(std::string_view name)
    {
        if (g_Data.EventHashStack.empty())
        {
            return std::hash<std::string_view>{}(name);
        }

        auto& parentEventInfo = g_Data.EventInfos[g_Data.EventHashStack.top()];
        parentEventInfo.IsParent = true;

        return HashCombine(g_Data.EventHashStack.top(), name);
    }

    static auto CreateOrUpdateEventInfo(std::string_view name)
    {
        const uint64_t hash = CalcEventHash(name);

        auto&& [it, _] = g_Data.EventInfos.try_emplace(hash, name.data(), (uint8_t)g_Data.EventHashStack.size());
        auto& eventInfo = it->second;

        if (!eventInfo.IsProcessed)
        {
            g_Data.SortCounter = std::max(eventInfo.SortIndex, g_Data.SortCounter);

            eventInfo.IsProcessed = true;
            eventInfo.SortIndex = g_Data.SortCounter++;

            const uint64_t prevHash = std::exchange(g_Data.SortedEventHashes[eventInfo.SortIndex], hash);
            if (prevHash != 0 && prevHash != hash)
            {
                g_Data.EventInfos.erase(prevHash);
            }

            BenzinAssert(g_Data.EventInfos.size() == g_Data.SortedEventHashes.size());
        }

        return std::pair<uint64_t, decltype(eventInfo)>{ hash, eventInfo };
    }

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
        BenzinEnsure(!name.empty());
        BenzinEnsure(g_Data.EventInfos.size() < ProfilerData::s_MaxEventCount);

        auto&& [hash, eventInfo] = CreateOrUpdateEventInfo(name);

        g_Data.EventHashStack.push(hash);

        eventInfo.BeginTimePoint = std::chrono::high_resolution_clock::now();
    }

    void Profiler::EndScope()
    {
        const auto endTimePoint = std::chrono::high_resolution_clock::now();

        auto& eventInfo = g_Data.EventInfos[g_Data.EventHashStack.top()];
        eventInfo.Us += std::chrono::duration_cast<std::chrono::microseconds>(endTimePoint - eventInfo.BeginTimePoint);

        g_Data.EventHashStack.pop();
    }

    void Profiler::SortEvents()
    {
        const bool isNeedResize = g_Data.SortedEvents.size() != g_Data.EventInfos.size();
        if (isNeedResize)
        {
            g_Data.SortedEvents.resize(g_Data.EventInfos.size());
        }

        for (auto& [_, eventInfo] : g_Data.EventInfos)
        {
            eventInfo.IsProcessed = false;

            auto& sortedEvent = g_Data.SortedEvents[eventInfo.SortIndex];
            sortedEvent.Us = std::exchange(eventInfo.Us, std::chrono::microseconds::zero());

            if (isNeedResize)
            {
                sortedEvent.Name = eventInfo.Name;
                sortedEvent.Depth = eventInfo.Depth;
                sortedEvent.IsParent = eventInfo.IsParent;
            }
        }
    }

    std::span<const ProfileEvent> Profiler::GetSortedEvents()
    {
        return g_Data.SortedEvents;
    }

}
