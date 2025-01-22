#pragma once

namespace benzin
{

    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;

    template <std::unsigned_integral T>
    inline constexpr auto g_InvalidUnsigned = std::numeric_limits<T>::max();

    template <typename T> requires std::is_enum_v<T>
    inline constexpr auto g_InvalidEnum = (T)g_InvalidUnsigned<std::underlying_type_t<T>>;

    template <std::unsigned_integral T>
    constexpr bool IsValidUnsigned(T value)
    {
        return value != g_InvalidUnsigned<T>;
    }

    template <typename T> requires std::is_enum_v<T>
    constexpr bool IsValidEnum(T value)
    {
        return value != g_InvalidEnum<T>;
    }

    template <std::unsigned_integral T, std::unsigned_integral U>
    constexpr auto GetValidUnsignedOr(T value, U orValue)
    {
        return (std::common_type_t<T, U>)(IsValidUnsigned(value) ? value : orValue);
    }

    template <std::unsigned_integral T>
    struct IndexRange
    {
        T StartIndex = 0;
        T Count = 0;

        IndexRange() = default;

        IndexRange(T startIndex)
            : StartIndex{ startIndex }
            , Count{ 1 }
        {}

        IndexRange(T startIndex, T count)
            : StartIndex{ startIndex }
            , Count{ count }
        {}
    };

    using IndexRange16 = IndexRange<uint16_t>;
    using IndexRange32 = IndexRange<uint32_t>;

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
