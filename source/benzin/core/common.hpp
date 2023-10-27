#pragma once

namespace benzin
{

    template <typename>
    inline constexpr bool g_DependentFalse = false;

    template <std::integral T>
    using IterableRange = std::ranges::iota_view<T, T>;

    constexpr uint64_t KBToBytes(uint64_t kb)
    {
        return kb * 1024;
    }

    constexpr uint64_t MBToBytes(uint64_t mb)
    {
        return mb * 1024 * 1024;
    }

    constexpr uint64_t GBToBytes(uint64_t gb)
    {
        return gb * 1024 * 1024 * 1024;
    }

    constexpr uint64_t BytesToKB(uint64_t bytes)
    {
        return (bytes / 1024) + (bytes % 1024 != 0);
    }

    constexpr uint64_t BytesToMB(uint64_t bytes)
    {
        const uint64_t kb = BytesToKB(bytes);
        return (kb / 1024) + (kb % 1024 != 0);
    }

    constexpr uint64_t BytesToGB(uint64_t bytes)
    {
        const uint64_t mb = BytesToMB(bytes);
        return (mb / 1024) + (mb % 1024 != 0);
    }

    template <std::integral T, std::integral U>
    constexpr auto AlignAbove(T value, U alignment)
    {
        using CommonType = std::common_type_t<T, U>;

        const CommonType commonValue = value;
        const CommonType commonAlignment = alignment;

        return (commonValue + (commonAlignment - 1)) & ~(commonAlignment - 1);
    }

    constexpr auto ToBit(std::integral auto bitPosition)
    {
        return 1 << bitPosition;
    }

    template <typename T>
    class ExecuteOnScopeExit
    {
    public:
        ExecuteOnScopeExit(T&& lambda)
            : m_Lambda{ std::forward<T>(lambda) }
        {}

        ~ExecuteOnScopeExit()
        {
            m_Lambda();
        }

    private:
        T m_Lambda;
    };

} // namespace benzin

#define BenzinStringConcatenate(string1, string2) string1##string2
#define BenzinStringConcatenate2(string1, string2) BenzinStringConcatenate(string1, string2)

#define BenzinUniqueVariableName(name) BenzinStringConcatenate2(name, __LINE__)

#define BenzinExecuteOnScopeExit(labmda) ::benzin::ExecuteOnScopeExit BenzinUniqueVariableName(_executeOnScopeExit){ labmda }
