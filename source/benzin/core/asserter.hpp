#pragma once

#define BENZIN_IS_ASSERTS_ENABLED BENZIN_IS_DEBUG_BUILD

namespace benzin
{

    class Asserter
    {
    public:
        template <typename... Args>
        friend struct Assert;

        using DeviceRemovedCallback = std::function<HRESULT()>;

        BenzinDefineNonConstructable(Asserter);

    public:
        static void SetDeviceRemovedCallback(DeviceRemovedCallback&& callback);

    private:
        static void AssertImpl(bool isPassed, std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message);
        static void AssertImpl(HRESULT hr, std::string_view conditionString, const std::source_location& sourceLocation, std::string_view message);
    };

    template <typename... Args>
    struct Assert
    {
        explicit Assert(bool isPassed, std::string_view conditionString, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Asserter::AssertImpl(isPassed, conditionString, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }

        explicit Assert(const void* const pointer, std::string_view conditionString, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Asserter::AssertImpl(pointer != nullptr, conditionString, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }

        explicit Assert(HRESULT hr, std::string_view conditionString, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            Asserter::AssertImpl(hr, conditionString, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }
    };

    template <typename... Args>
    Assert(bool, std::string_view, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(const void* const, std::string_view, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(HRESULT, std::string_view, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

} // namespace benzin

#if BENZIN_IS_ASSERTS_ENABLED
    #define BenzinAssert(condition, ...) benzin::Assert{ condition, #condition, __VA_ARGS__ }
    #define BenzinAssertExpr(expression, ...) BenzinAssert(expression, __VA_ARGS__)
#else
    #define BenzinAssert(condition, ...)
    #define BenzinAssertExpr(expression, ...) BenzinUnused(expression)
#endif

#define BenzinEnsure(condition, ...) benzin::Assert{ condition, #condition, __VA_ARGS__ }
