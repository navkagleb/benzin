#include <benzin/config/bootstrap.hpp>
#include <benzin/core/profiler.hpp>

#include <benzin/utility/time_utils.hpp>

namespace benzin
{

    struct EventInfo
    {
        const char* Name = nullptr;

        uint8_t Depth : 6 = 0;
        uint8_t IsParent : 1 = false;
        uint8_t IsProcessed : 1 = false;
        uint8_t SortIndex = 0;

        std::chrono::microseconds Us = std::chrono::microseconds::zero();
    };

    struct EventStackInfo
    {
        uint64_t Hash = 0;
        std::chrono::high_resolution_clock::time_point BeginTimePoint{};
    };

    struct ProfilerData
    {
        static constexpr auto s_MaxEventCount = std::numeric_limits<uint8_t>::max();

        std::unordered_map<uint64_t, EventInfo> HashToEventInfo;
        std::unordered_map<uint8_t, uint64_t> SortIndexToHash; // TODO: Try to replace with std::vector
        std::stack<EventStackInfo, std::vector<EventStackInfo>> EventStack;

        std::vector<ProfileEvent> SortedEvents;

        uint8_t CurrentSortIndex = 0;
    };

    static ProfilerData g_Data;

    static uint64_t CalcEventHash(std::string_view name)
    {
        if (g_Data.EventStack.empty())
        {
            return std::hash<std::string_view>{}(name);
        }

        const uint64_t parentHash = g_Data.EventStack.top().Hash;

        g_Data.HashToEventInfo[parentHash].IsParent = true;

        return HashCombine(parentHash, name);
    }

    static uint64_t CreateOrUpdateEventInfo(std::string_view name)
    {
        BenzinEnsure(!name.empty());
        BenzinEnsure(g_Data.HashToEventInfo.size() < ProfilerData::s_MaxEventCount);

        const uint64_t hash = CalcEventHash(name);

        auto&& [it, _] = g_Data.HashToEventInfo.try_emplace(hash, name.data(), (uint8_t)g_Data.EventStack.size());
        auto& eventInfo = it->second;

        if (!eventInfo.IsProcessed)
        {
            g_Data.CurrentSortIndex = std::max(eventInfo.SortIndex, g_Data.CurrentSortIndex);

            eventInfo.IsProcessed = true;
            eventInfo.SortIndex = g_Data.CurrentSortIndex++;

            const uint64_t prevHash = std::exchange(g_Data.SortIndexToHash[eventInfo.SortIndex], hash);
            if (prevHash != 0 && prevHash != hash)
            {
                g_Data.HashToEventInfo.erase(prevHash);
            }

            BenzinAssert(g_Data.HashToEventInfo.size() == g_Data.SortIndexToHash.size());
        }

        return hash;
    }

    // Profiler

    void Profiler::Initialize()
    {
        g_Data.SortedEvents.reserve(ProfilerData::s_MaxEventCount);
    }

    void Profiler::BeginFrame()
    {
        BenzinAssert(g_Data.EventStack.empty());

        g_Data.CurrentSortIndex = 0;
    }

    void Profiler::EndFrame()
    {
        BenzinAssert(g_Data.EventStack.empty());

        SortEvents();
    }

    std::span<const ProfileEvent> Profiler::GetSortedEvents()
    {
        return g_Data.SortedEvents;
    }

    void Profiler::BeginScope(std::string_view name)
    {
        const uint64_t hash = CreateOrUpdateEventInfo(name);

        g_Data.EventStack.push(EventStackInfo
        {
            .Hash = hash,
            .BeginTimePoint = std::chrono::high_resolution_clock::now(),
        });
    }

    void Profiler::EndScope()
    {
        BenzinAssert(!g_Data.EventStack.empty());

        const auto endTimePoint = std::chrono::high_resolution_clock::now();
        const auto& stackEventInfo = g_Data.EventStack.top();

        auto& eventInfo = g_Data.HashToEventInfo[stackEventInfo.Hash];
        eventInfo.Us += std::chrono::duration_cast<std::chrono::microseconds>(endTimePoint - stackEventInfo.BeginTimePoint);

        g_Data.EventStack.pop();
    }

    void Profiler::SortEvents()
    {
        const auto eventCount = g_Data.HashToEventInfo.size();

        const bool isNeedResize = g_Data.SortedEvents.size() != eventCount;
        if (isNeedResize)
        {
            g_Data.SortedEvents.resize(eventCount);
        }

        for (auto& [_, eventInfo] : g_Data.HashToEventInfo)
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

    // ScopedProfileEvent

    ScopedProfileEvent::ScopedProfileEvent(std::string_view name)
    {
        Profiler::BeginScope(name);
    }

    ScopedProfileEvent::~ScopedProfileEvent()
    {
        Profiler::EndScope();
    }

}
