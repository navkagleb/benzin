#pragma once

namespace benzin
{

    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;

    template <EnumConcept T>
    struct IsAllowedFlagsForEnum : std::false_type {};

    template <EnumConcept T>
    struct IsAllowedFlagsForBitEnum : std::false_type {};

    template <EnumConcept T>
    class EnumFlags
    {
    public:
        using UnderlyingT = std::underlying_type_t<T>;

        EnumFlags() = default;

        EnumFlags(T flag)
            : m_Bits{ ToBit(flag) }
        {}

        auto GetRawBits() const
        {
            return m_Bits;
        }

        void Set(T flag)
        {
            m_Bits |= ToBit(flag);
        }

        bool IsSet(T flag) const
        {
            const auto bit = ToBit(flag);
            return (m_Bits & bit) == bit;
        }

        bool IsAnySet(EnumFlags<T> flags) const
        {
            return (m_Bits & flags.m_Bits) != 0;
        }

        auto operator|(EnumFlags other) const
        {
            EnumFlags result;
            result.m_Bits = m_Bits | other.m_Bits;

            return result;
        }

    private:
        static UnderlyingT ToBit(T flag)
        {
            if constexpr (IsAllowedFlagsForBitEnum<T>::value)
            {
                return (UnderlyingT)flag;
            }
            else
            {
                return (UnderlyingT)1 << (UnderlyingT)flag;
            }
        }

        UnderlyingT m_Bits = 0;
    };

} // namespace benzin

template <benzin::EnumConcept T> requires benzin::IsAllowedFlagsForEnum<T>::value
auto operator|(T first, T second)
{
    return benzin::EnumFlags<T>{ first } | benzin::EnumFlags<T>{ second };
}

#define BenzinDefineFlagsForEnum(EnumTypeName) \
    template <> \
    struct benzin::IsAllowedFlagsForEnum<EnumTypeName> : std::true_type {}; \
    using BenzinStringConcatenate2(EnumTypeName, s) = benzin::EnumFlags<EnumTypeName>

#define BenzinDefineFlagsForBitEnum(EnumTypeName) \
    template <> \
    struct benzin::IsAllowedFlagsForBitEnum<EnumTypeName> : std::true_type {}; \
    BenzinDefineFlagsForEnum(EnumTypeName)
