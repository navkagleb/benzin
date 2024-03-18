#pragma once

namespace benzin
{

    template <typename T>
    using UniquePtrInnerType = T::element_type;

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

    template <std::unsigned_integral T>
    constexpr bool IsValidIndex(T index)
    {
        return index != g_InvalidIndex<T>;
    }

    inline constexpr auto g_InvalidEntity = (entt::entity)g_InvalidIndex<std::underlying_type_t<entt::entity>>;

    template <typename... Fs>   
    struct VisitorMatch : Fs...
    {
        using Fs::operator()...;
    };

    template <typename... Fs>
    VisitorMatch(Fs...) -> VisitorMatch<Fs...>;

    template <typename T>
    concept Enum = std::is_enum_v<T>;

    template <std::unsigned_integral T> 
    struct IndexRange
    {
        T StartIndex = 0;
        T Count = 0;
    };

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

    template <Enum T>
    auto EnumIota()
    {
        return std::views::iota(0u, magic_enum::enum_count<T>());
    }

    template <typename From, size_t Size, typename Transformator, size_t... Is>
    auto TransformArray(const std::array<From, Size>& from, Transformator&& transformator, std::index_sequence<Is...>)
    {
        return std::to_array({ transformator(from[Is])... });
    }

    template <typename From, size_t Size, typename Transformator, size_t... Is>
    auto TransformArray(const std::array<From, Size>& from, Transformator&& transformator)
    {
        return TransformArray(from, transformator, std::make_index_sequence<Size>());
    }

    template <Enum Enum, typename From, typename Transformator, size_t... Is>
    auto TransformArray(const magic_enum::containers::array<Enum, From>& from, Transformator&& transformator, std::index_sequence<Is...>)
    {
        return magic_enum::containers::to_array<Enum>({ transformator(from.a[Is])... });
    }

    template <Enum Enum, typename From, typename Transformator, size_t... Is>
    auto TransformArray(const magic_enum::containers::array<Enum, From>& from, Transformator&& transformator)
    {
        return TransformArray(from, transformator, std::make_index_sequence<magic_enum::enum_count<Enum>()>());
    }

} // namespace benzin

#define BenzinExecuteOnScopeExit(lambda) const ::benzin::ExecuteOnScopeExit BenzinUniqueVariableName(_executeOnScopeExit){ lambda }
