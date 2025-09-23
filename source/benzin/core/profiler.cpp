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

        ProfileNode m_Root; // Fake root node
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
        ProfileNode* node = parent->GetAndUpdateChild(name);

        node->m_HitCount++;

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

    const ProfileNode* Profiler::GetRootNode()
    {
        const auto& children = g_Data.m_Root.m_Children;

        if (children.empty())
            return nullptr;

        BenzinAssert(children.size() == 1);
        return children.front().get();
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
