#pragma once

template <benzin::EnumConcept T>
struct IsFlagsEnabledForEnum : std::false_type {};

template <benzin::EnumConcept T>
struct IsFlagsEnabledForBitEnum : std::false_type {};

namespace benzin
{

    template <EnumConcept T>
    class EnumFlags
    {
    public:
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

        bool IsAnySet(EnumFlags flags) const
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
        using UnderlyingTypeT = std::underlying_type_t<T>;

        static UnderlyingTypeT ToBit(T flag)
        {
            if constexpr (IsFlagsEnabledForBitEnum<T>::value)
            {
                return (UnderlyingTypeT)flag;
            }
            else
            {
                return (UnderlyingTypeT)1 << (UnderlyingTypeT)flag;
            }
        }

        UnderlyingTypeT m_Bits = 0;
    };

} // namespace benzin

template <typename T> requires IsFlagsEnabledForEnum<T>::value
auto operator|(T first, T second)
{
    return benzin::EnumFlags<T>{ first } | benzin::EnumFlags<T>{ second };
}

#define BenzinEnableFlagsForEnum(EnumTypeName) \
    template <> \
    struct IsFlagsEnabledForEnum<EnumTypeName> : std::true_type {}; \
    using BenzinStringConcatenate2(EnumTypeName, s) = benzin::EnumFlags<EnumTypeName>

#define BenzinEnableFlagsForBitEnum(EnumTypeName) \
    template <> \
    struct IsFlagsEnabledForBitEnum<EnumTypeName> : std::true_type {}; \
    BenzinEnableFlagsForEnum(EnumTypeName)
