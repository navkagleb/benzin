#pragma once

namespace benzin
{

    template <typename T, typename DurationT>
    struct ProfileNodeBase
    {
        using Duration = DurationT;

        T* m_Parent = nullptr;
        mutable std::vector<std::unique_ptr<T>> m_Children;
        mutable bool m_IsSortingNeeded = false;

        std::string_view m_Name;
        std::unordered_map<std::string_view, T*> m_ChildrenMap;

        DurationT m_Duration = {};
        DurationT m_AccumulatedDuration = {};
        DurationT m_ImGuiDuration = {};

        uint32_t m_CurrentChildOffset = 0;
        uint32_t m_SortIndex = 0;

        void SortChildren() const
        {
            BenzinAssert(m_IsSortingNeeded);

            std::ranges::sort(m_Children, [](const std::unique_ptr<T>& lhs, const std::unique_ptr<T>& rhs)
            {
                return lhs->m_SortIndex < rhs->m_SortIndex;
            });

            m_IsSortingNeeded = false;
        }

        T* GetAndUpdateChild(std::string_view name)
        {
            T* child = GetOrAddChild(name);

            if (child->m_SortIndex < m_CurrentChildOffset)
            {
                m_IsSortingNeeded = true;
                child->m_SortIndex = m_CurrentChildOffset;
            }

            m_CurrentChildOffset++;

            return child;
        }

    private:
        T* GetOrAddChild(std::string_view name)
        {
            auto it = m_ChildrenMap.find(name);
            if (it != m_ChildrenMap.end())
                return it->second;

            auto child = std::make_unique<T>();
            child->m_Name = name;
            child->m_Parent = (T*)this;

            T* rawChild = child.get();

            m_Children.push_back(std::move(child));
            m_ChildrenMap[name] = rawChild;

            return rawChild;
        }
    };

    struct ProfileNode : ProfileNodeBase<ProfileNode, std::chrono::high_resolution_clock::duration>
    {
        using Clock = std::chrono::high_resolution_clock; // TODO: Replace with QueryPerformanceCounter
        using TimePoint = Clock::time_point;

        uint32_t m_HitCount = 0;
        uint32_t m_ImGuiHitCount = 0;
    };

    class Profiler
    {
    public:
        static void BeginFrame();
        static void EndFrame();

        static void ResetAccumulatedData(uint32_t frameCount);

        static const ProfileNode* GetRootNode();
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
