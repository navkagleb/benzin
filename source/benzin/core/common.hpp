#pragma once

#if !defined(BenzinAssert)
    #error
#endif

namespace benzin
{

    template <typename>
    inline constexpr bool g_DependentFalse = false;

    template <std::unsigned_integral T>
    inline constexpr auto g_InvalidIndex = std::numeric_limits<T>::max();

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

    template <std::integral T>
    using IterableRange = std::ranges::iota_view<T, T>;

    using Seconds = std::chrono::duration<float>;
    using MilliSeconds = std::chrono::duration<float, std::milli>;

    struct IndexRange
    {
        size_t StartIndex = 0;
        size_t Count = 0;
    };

    using ByteSpan = std::span<std::byte>;

    constexpr uint64_t KibiBytesToBytes(uint64_t kb)
    {
        return kb * 1024;
    }

    constexpr uint64_t MebiBytesToBytes(uint64_t mb)
    {
        return mb * 1024 * 1024;
    }

    constexpr uint64_t GibiBytesToBytes(uint64_t gb)
    {
        return gb * 1024 * 1024 * 1024;
    }

    constexpr uint64_t BytesToKibiBytes(uint64_t bytes)
    {
        return (bytes / 1024) + (bytes % 1024 != 0);
    }

    constexpr uint64_t BytesToMebiBytes(uint64_t bytes)
    {
        const uint64_t kb = BytesToKibiBytes(bytes);
        return (kb / 1024) + (kb % 1024 != 0);
    }

    constexpr uint64_t BytesToGibiBytes(uint64_t bytes)
    {
        const uint64_t mb = BytesToMebiBytes(bytes);
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

    constexpr size_t HashCombine(const auto& value, size_t resultHash)
    {
        const auto hashedValue = std::hash<std::decay_t<decltype(value)>>{}(value);
        const auto combinedHash = resultHash ^ hashedValue + 0x9e3779b9 + (resultHash << 6) + (resultHash >> 2);
        
        return combinedHash;
    }

    template <typename T>
    class SingletonInstanceWrapper
    {
    public:
        BenzinDefineNonConstructable(SingletonInstanceWrapper);

    public:
        static bool IsInitialized()
        {
            return static_cast<bool>(ms_Instance);
        }

        static const T& Initialize(auto&&... args)
        {
            BenzinAssert(!IsInitialized());
            ms_Instance = std::make_unique<T>(std::forward<decltype(args)>(args)...);

            return *ms_Instance;
        }

        static const T& Get()
        {
            BenzinAssert(IsInitialized());
            return *ms_Instance;
        }

    private:
        inline static std::unique_ptr<T> ms_Instance;
    };

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

#define BenzinExecuteOnScopeExit(lambda) ::benzin::ExecuteOnScopeExit BenzinUniqueVariableName(_executeOnScopeExit){ lambda }

// typeof(value) == float || MilliSecond
#define BenzinAsSeconds(value) ::benzin::Seconds{ value }

// typeof(value) == float || Second
#define BenzinAsMilliSeconds(value) ::benzin::MilliSeconds{ value }

template <typename... Ts, typename... Fs>
constexpr decltype(auto) operator |(const std::variant<Ts...>& variant, const benzin::VisitorMatch<Fs...>& visitorMatch)
{
    return std::visit(visitorMatch, variant);
}
