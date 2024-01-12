#pragma once

#if !defined(BENZIN_IS_DEBUG_BUILD)
    #error
#endif

#define BENZIN_IS_ASSERTS_ENABLED BENZIN_IS_DEBUG_BUILD

namespace benzin
{

    void AssertFormat(const std::source_location& sourceLocation, std::string_view message);
    void AssertFormat(HRESULT hr, const std::source_location& sourceLocation, std::string_view message);

    inline std::string ArgsToFormat()
    {
        return ""s;
    }

    inline std::string ArgsToFormat(std::nullptr_t)
    {
        return ""s;
    }

    inline std::string ArgsToFormat(std::string_view format, auto&&... args)
    {
        return std::vformat(format, std::make_format_args(args...));
    }

    inline void AssertImpl(bool condition, const std::source_location& sourceLocation, auto&&... args)
    {
        if (!condition)
        {
            AssertFormat(sourceLocation, ArgsToFormat(std::forward<decltype(args)>(args)...));
        }
    }

    inline void AssertImpl(HRESULT hr, const std::source_location& sourceLocation, auto&&... args)
    {
        if (FAILED(hr))
        {
            AssertFormat(hr, sourceLocation, ArgsToFormat(std::forward<decltype(args)>(args)...));
        }
    }

    template <typename... Args>
    struct Assert
    {
        explicit Assert(bool condition, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            AssertImpl(condition, sourceLocation, std::forward<Args>(args)...);
#endif
        }

        explicit Assert(const void* const pointer, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            AssertImpl(pointer != nullptr, sourceLocation, std::forward<Args>(args)...);
#endif
        }

        explicit Assert(HRESULT hr, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
#if BENZIN_IS_ASSERTS_ENABLED
            AssertImpl(hr, sourceLocation, std::forward<Args>(args)...);
#endif
        }
    };

    template <typename... Args>
    struct Ensure
    {
        explicit Ensure(bool condition, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AssertImpl(condition, sourceLocation, std::forward<Args>(args)...);
        }

        explicit Ensure(const void* const pointer, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AssertImpl(pointer != nullptr, sourceLocation, std::forward<Args>(args)...);
        }

        explicit Ensure(HRESULT hr, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AssertImpl(hr, sourceLocation, std::forward<Args>(args)...);
        }
    };

    template <typename... Args>
    Assert(bool, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(const void* const, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(HRESULT, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Ensure(bool, Args&&...) -> Ensure<Args...>;

    template <typename... Args>
    Ensure(const void* const, Args&&...) -> Ensure<Args...>;

    template <typename... Args>
    Ensure(HRESULT, Args&&...) -> Ensure<Args...>;

} // namespace benzin

#define BenzinAssert(condition, ...) ::benzin::Assert{ condition, __VA_ARGS__ }
#define BenzinEnsure(condition, ...) ::benzin::Ensure{ condition, __VA_ARGS__ }
