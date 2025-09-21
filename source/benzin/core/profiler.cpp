#include <benzin/config/bootstrap.hpp>
#include <benzin/core/profiler.hpp>

namespace benzin
{

    static std::string_view GetClassAndFunction(std::string_view fullSignature)
    {
        size_t lastNamespaceSeparator = fullSignature.rfind("::");
        if (lastNamespaceSeparator != std::string_view::npos)
        {
            size_t prevSeparator = fullSignature.rfind("::", lastNamespaceSeparator - 1);
            if (prevSeparator != std::string_view::npos)
            {
                fullSignature.remove_prefix(prevSeparator + 2);
            }
        }

        size_t openParenthesis = fullSignature.find('(');
        if (openParenthesis != std::string_view::npos)
        {
            fullSignature.remove_suffix(fullSignature.size() - openParenthesis);
        }

        return fullSignature;
    }

    struct ProfilerData
    {
        struct StackEntry
        {
            ProfileNode* m_Node = nullptr;
            ProfileNode::TimePoint m_BeginTimePoint = {};
        };

        ProfileNode m_Root;
        std::stack<StackEntry> m_NodeStack;

        ProfilerData()
        {
            m_Root.m_Name = "Root";
        }

        void PushCurrentNode(ProfileNode* node)
        {
            StackEntry& stackEntry = m_NodeStack.emplace();
            stackEntry.m_Node = node;
            stackEntry.m_BeginTimePoint = ProfileNode::Clock::now();
        }

        void PopCurrentNode()
        {
            BenzinAssert(!m_NodeStack.empty());
            
            const ProfileNode::TimePoint endTimePoint = ProfileNode::Clock::now();

            const StackEntry& stackEntry = m_NodeStack.top();
            m_NodeStack.pop();

            stackEntry.m_Node->m_Duration += endTimePoint - stackEntry.m_BeginTimePoint;
        }
    };

    static ProfilerData g_Data;

    static ProfileNode* GetOrCreateEvent(std::string_view name)
    {
        if (g_Data.m_NodeStack.empty())
            return nullptr;

        ProfileNode* parent = g_Data.m_NodeStack.top().m_Node;
        ProfileNode* node = nullptr;

        auto it = parent->m_ChildrenMap.find(name);
        if (it != parent->m_ChildrenMap.end())
        {
            node = it->second;
        }
        else
        {
            auto newNode = std::make_unique<ProfileNode>();
            newNode->m_Name = name;
            newNode->m_Parent = parent;

            node = newNode.get();
            parent->m_Children.push_back(std::move(newNode));
            parent->m_ChildrenMap[name] = node;
        }

        BenzinAssert(node != nullptr);

        node->m_HitCount++;

        if (node->m_SortIndex < parent->m_CurrentChildOffset)
        {
            parent->m_IsSortingNeeded = true;
            node->m_SortIndex = parent->m_CurrentChildOffset;
        }
        parent->m_CurrentChildOffset++;

        return node;
    }

    static void ResetFrameDataRecursive(ProfileNode& node)
    {
        node.m_CurrentChildOffset = 0;

        node.m_ImGuiHitCount = node.m_HitCount;
        node.m_AccumulatedDuration += node.m_Duration;

        node.m_HitCount = 0;
        node.m_Duration = {};

        for (auto& child : node.m_Children)
        {
            ResetFrameDataRecursive(*child);
        }
    }

    static void ResetAccumulatedDurationRecursive(ProfileNode& node, uint32_t frameCount)
    {
        node.m_ImGuiDuration = std::exchange(node.m_AccumulatedDuration, {}) / frameCount;

        for (auto& child : node.m_Children)
        {
            ResetAccumulatedDurationRecursive(*child, frameCount);
        }
    }

    // ProfileNode

    void ProfileNode::SortChildren() const
    {
        BenzinAssert(m_IsSortingNeeded);

        std::ranges::sort(m_Children, [](const std::unique_ptr<ProfileNode>& lhs, const std::unique_ptr<ProfileNode>& rhs)
        {
            return lhs->m_SortIndex < rhs->m_SortIndex;
        });

        m_IsSortingNeeded = false;
    }

    // Profiler

    void Profiler::BeginFrame()
    {
        if (g_Data.m_NodeStack.empty())
        {
            g_Data.m_NodeStack.emplace(&g_Data.m_Root);
        }

        BenzinAssert(g_Data.m_NodeStack.top().m_Node == &g_Data.m_Root);
    }

    void Profiler::EndFrame()
    {
        BenzinAssert(g_Data.m_NodeStack.top().m_Node == &g_Data.m_Root);

        ResetFrameDataRecursive(g_Data.m_Root);
    }

    void Profiler::ResetAccumulatedData(uint32_t frameCount)
    {
        ResetAccumulatedDurationRecursive(g_Data.m_Root, frameCount);
    }

    const ProfileNode& Profiler::GetRootNode()
    {
        return g_Data.m_Root;
    }

    // ScopedProfileEvent

    ScopedProfileEvent::ScopedProfileEvent(std::string_view name)
    {
        name = GetClassAndFunction(name);
        m_Node = GetOrCreateEvent(name);

        if (m_Node != nullptr)
        {
            g_Data.PushCurrentNode(m_Node);
        }
    }

    ScopedProfileEvent::~ScopedProfileEvent()
    {
        if (m_Node != nullptr)
        {
            g_Data.PopCurrentNode();
        }
    }

}
