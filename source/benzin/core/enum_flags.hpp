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
    private:
        using UnderlyingTypeT = std::underlying_type_t<T>;

    public:
        EnumFlags(UnderlyingTypeT rawBits = 0)
            : m_Bits{ rawBits }
        {}

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

        UnderlyingTypeT m_Bits;
    };

}

template <typename T> requires IsFlagsEnabledForEnum<T>::value
auto operator|(T first, T second)
{
    return benzin::EnumFlags<T>{ first } | benzin::EnumFlags<T>{ second };
}

#define BenzinEnableFlagsForEnum(enumTypeName) \
    template <> struct IsFlagsEnabledForEnum<enumTypeName> : std::true_type {}

#define BenzinEnableFlagsForBitEnum(enumTypeName) \
    template <> struct IsFlagsEnabledForBitEnum<enumTypeName> : std::true_type {}; \
    BenzinEnableFlagsForEnum(enumTypeName)

template <benzin::EnumConcept T>
struct IsDereferenceOperatorEnabledForEnum : std::false_type {};

template <benzin::EnumConcept T> requires IsDereferenceOperatorEnabledForEnum<T>::value
constexpr auto operator*(T value)
{
    return magic_enum::enum_integer(value);
}

#define BenzinAllowDereferenceOperatorForEnum(EnumT) \
    template <> \
    struct IsDereferenceOperatorEnabledForEnum<EnumT> : std::true_type {}
