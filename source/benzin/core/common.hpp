#pragma once

namespace benzin
{

    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;

    template <std::unsigned_integral T>
    inline constexpr auto g_BadUint = std::numeric_limits<T>::max();

    inline constexpr auto g_Bad16 = g_BadUint<uint16_t>;
    inline constexpr auto g_Bad32 = g_BadUint<uint32_t>;
    inline constexpr auto g_Bad64 = g_BadUint<uint64_t>;

    template <std::unsigned_integral T>
    constexpr bool IsGoodUint(T value)
    {
        return value != g_BadUint<T>;
    }

    template <std::unsigned_integral T, std::unsigned_integral U>
    constexpr auto GetGoodUintOr(T value, U orValue)
    {
        return (std::common_type_t<T, U>)(IsGoodUint(value) ? value : orValue);
    }

    template <EnumConcept T>
    inline constexpr auto g_BadEnum = (T)g_BadUint<std::underlying_type_t<T>>;

    template <EnumConcept T>
    constexpr bool IsGoodEnum(T value)
    {
        return value != g_BadEnum<T>;
    }

    template <std::unsigned_integral T>
    struct SubRange
    {
        T Offset = 0;
        T Count = g_BadUint<T>;

        SubRange() = default;

        SubRange(T offset)
            : Offset{ offset }
            , Count{ 1 }
        {}

        SubRange(T offset, T count)
            : Offset{ offset }
            , Count{ count }
        {}

        template <std::unsigned_integral U>
        SubRange(const SubRange<U>& other)
        {
            static_assert(sizeof(U) <= sizeof(T));

            Offset = (T)other.Offset;
            Count = (T)other.Count;
        }

        bool IsGoodRange() const
        {
            return IsGoodUint(Count);
        }

        T GetEndCount() const
        {
            return Offset + Count;
        }
    };

    using SubRange16 = SubRange<uint16_t>;
    using SubRange32 = SubRange<uint32_t>;
    using SubRange64 = SubRange<uint64_t>;

    constexpr auto ToBit(std::integral auto bitPosition)
    {
        return 1 << bitPosition;
    }

    constexpr size_t HashCombine(size_t resultHash, const auto& value)
    {
        const auto hashedValue = std::hash<std::decay_t<decltype(value)>>{}(value);
        const auto combinedHash = resultHash ^ hashedValue + 0x9e3779b9 + (resultHash << 6) + (resultHash >> 2);

        return combinedHash;
    }

    template <typename T>
    class ExecuteOnScopeExit
    {
    public:
        ExecuteOnScopeExit(T&& lambda)
            : m_Lambda{ std::move(lambda) }
        {}

        ~ExecuteOnScopeExit()
        {
            m_Lambda();
        }

    private:
        const T m_Lambda;
    };

    template <typename T>
    std::string_view GetClassName()
    {
        std::string_view name = typeid(T).name();

        // Removes namespace name
        const size_t lastDoubleColonPosition = name.find_last_of("::");
        if (lastDoubleColonPosition != std::string_view::npos)
        {
            name = name.substr(lastDoubleColonPosition + 1);
        }

        return name;
    }

    template <typename T, uint32_t _MaxElementCount>
    class StaticArray
    {
    public:
        void Add(T&& element)
        {
            BenzinAssert(m_Count < _MaxElementCount);
            m_Elements[m_Count++] = std::forward<T>(element);
        }

        auto Get() const
        {
            return ToSpan(m_Elements.data(), m_Count);
        }

    private:
        std::array<T, _MaxElementCount> m_Elements{};
        uint32_t m_Count = 0;
    };

}

#define BenzinExecuteOnScopeExit(lambda) const benzin::ExecuteOnScopeExit BenzinUniqueVariableName(_executeOnScopeExit){ lambda }

template <benzin::EnumConcept T>
struct IsUnaryPlusEnabledForEnum : std::false_type {};

template <benzin::EnumConcept T> requires IsUnaryPlusEnabledForEnum<T>::value
constexpr auto operator+(T enumValue)
{
    return magic_enum::enum_integer(enumValue);
}

#define BenzinEnableUnaryPlusForEnum(EnumT) \
    template <> \
    struct IsUnaryPlusEnabledForEnum<EnumT> : std::true_type {};
