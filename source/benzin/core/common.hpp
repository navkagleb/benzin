#pragma once

namespace benzin
{

    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;

    template <typename T, EnumConcept EnumT>
    using EnumArray = std::array<T, magic_enum::enum_count<EnumT>()>;

    template <typename UniquePtrT, typename... Args>
    void MakeUniquePtr(UniquePtrT& outUniquePtr, Args&&... args)
    {
        using InnerType = std::decay_t<UniquePtrT>::element_type;
        outUniquePtr = std::make_unique<InnerType>(std::forward<Args>(args)...);
    }

    template <typename>
    inline constexpr bool g_DependentFalse = false;

    template <std::unsigned_integral T>
    inline constexpr auto g_InvalidIndex = std::numeric_limits<T>::max();

    template <typename T> requires std::is_enum_v<T>
    inline constexpr auto g_InvalidEnumValue = (T)g_InvalidIndex<std::underlying_type_t<T>>;

    template <std::unsigned_integral T>
    constexpr bool IsValidIndex(T index)
    {
        return index != g_InvalidIndex<T>;
    }

    template <typename... Fs>
    struct VisitorMatch : Fs...
    {
        using Fs::operator()...;
    };

    template <typename... Fs>
    auto MakeVisitorMatch(Fs... lambdas)
    {
        return VisitorMatch<Fs...>{ lambdas... };
    }

    template <typename T>
    auto ToSingleSpan(const T& value)
    {
        return std::span{ &value, 1 };
    }

    template <std::unsigned_integral T>
    struct IndexRange
    {
        T StartIndex = 0;
        T Count = 0;
    };

    using IndexRangeU16 = IndexRange<uint16_t>;
    using IndexRangeU32 = IndexRange<uint32_t>;

    template <std::unsigned_integral T>
    constexpr auto IndexRangeToView(IndexRange<T> indexRange)
    {
        return std::ranges::iota_view{ indexRange.StartIndex, indexRange.StartIndex + indexRange.Count };
    }

    constexpr uint64_t KbToBytes(uint64_t kb) { return kb * 1024; }
    constexpr uint64_t MbToBytes(uint64_t mb) { return KbToBytes(mb * 1024); }
    constexpr uint64_t GbToBytes(uint64_t gb) { return MbToBytes(gb * 1024); }

    constexpr float BytesToFloatKb(uint64_t bytes) { return (float)bytes / KbToBytes(1); }
    constexpr float BytesToFloatMb(uint64_t bytes) { return (float)bytes / MbToBytes(1); }
    constexpr float BytesToFloatGb(uint64_t bytes) { return (float)bytes / GbToBytes(1); }

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

} // namespace benzin

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
