#pragma once

namespace benzin
{

    template <typename... Args>
    inline void AssertFormat(const std::source_location& sourceLocation, std::string_view format, Args&&... args)
    {
        BenzinError("Assert failed | {}:{} | {}", sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name());

        if (!format.empty())
        {
            BenzinError("Message: {}", fmt::format(format, std::forward<Args>(args)...));
        }

        __debugbreak();
    }

    inline void AssertFormat(const std::source_location& sourceLocation)
    {
        AssertFormat(sourceLocation, "");
    }

    template <typename... Args>
    inline void AssertFormat(HRESULT hr, const std::source_location& sourceLocation, std::string_view format, Args&&... args)
    {
        BenzinError("Assert failed | {}:{} | {}", sourceLocation.file_name(), sourceLocation.line(), sourceLocation.function_name());

        if (!format.empty())
        {
            BenzinError("Message: {}", fmt::format(format, std::forward<Args>(args)...));
        }

        _com_error comError{ hr };
        BenzinError("ComError message: {}", comError.ErrorMessage());

        __debugbreak();
    }

    inline void AssertFormat(HRESULT hr, const std::source_location& sourceLocation)
    {
        AssertFormat(hr, sourceLocation, "");
    }

    template <typename... Args>
    inline void AssertImpl(bool condition, const std::source_location& sourceLocation, Args&&... args)
    {
        if (!condition)
        {
            AssertFormat(sourceLocation, std::forward<Args>(args)...);
        }
    }

    template <typename... Args>
    inline void AssertImpl(HRESULT hr, const std::source_location& sourceLocation, Args&&... args)
    {
        if (FAILED(hr))
        {
            AssertFormat(hr, sourceLocation, std::forward<Args>(args)...);
        }
    }

    // Interface
    template <typename... Args>
    struct Assert
    {
        explicit Assert(bool condition, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            if constexpr (BENZIN_IS_DEBUG_BUILD)
            {
                AssertImpl(condition, sourceLocation, std::forward<Args>(args)...);
            }
        }

        explicit Assert(const void* const pointer, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            if constexpr (BENZIN_IS_DEBUG_BUILD)
            {
                AssertImpl(pointer != nullptr, sourceLocation, std::forward<Args>(args)...);
            }
        }

        explicit Assert(HRESULT hr, Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            if constexpr (BENZIN_IS_DEBUG_BUILD)
            {
                AssertImpl(hr, sourceLocation, std::forward<Args>(args)...);
            }
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
