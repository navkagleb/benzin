#pragma once

namespace benzin
{

    template <typename T>
    concept EnumConcept = std::is_enum_v<T>;

    template <typename T>
    inline constexpr T g_MaxUint = std::numeric_limits<T>::max();

    template <typename T>
    inline constexpr T g_MaxEnum = (T)g_MaxUint<std::underlying_type_t<T>>;

    template <typename T>
    constexpr bool IsMaxUint(T value)
    {
        return value == g_MaxUint<T>;
    }

    template <typename T>
    constexpr bool IsMaxEnum(T value)
    {
        return value == g_MaxEnum<T>;
    }

    inline constexpr uint32_t g_MaxU32 = g_MaxUint<uint32_t>;
    inline constexpr uint64_t g_MaxU64 = g_MaxUint<uint64_t>;

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

    template <typename CreateCallbackT>
    class LazyConverter
    {
    public:
        using ResultType = std::invoke_result_t<const CreateCallbackT&>;

        constexpr LazyConverter(CreateCallbackT&& callback)
            : m_Callback{ std::move(callback) }
        {}

        constexpr operator ResultType() const noexcept(std::is_nothrow_invocable_v<const CreateCallbackT&>)
        {
            return m_Callback();
        }

    private:
        CreateCallbackT m_Callback;
    };

    template <typename CreateCallbackT>
    auto MakeLazyConverter(CreateCallbackT&& callback)
    {
        return LazyConverter{ std::move(callback) };
    }

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
