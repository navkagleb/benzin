#pragma once

namespace benzin
{

    class Asserter
    {
    public:
        template <typename... Args>
        friend struct Assert;

        Asserter() = default;

        BenzinDefineNonCopyable(Asserter);
        BenzinDefineNonMoveable(Asserter);

    private:
        void AssertImpl(bool isPassed, const std::source_location& sourceLocation, std::string_view message) const;
        void AssertImpl(HRESULT hr, const std::source_location& sourceLocation, std::string_view message) const;
    };

    using AsserterInstance = SingletonInstanceWrapper<Asserter>;

    template <typename... Args>
    struct Assert
    {
        explicit Assert(bool isPassed, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AsserterInstance::Get().AssertImpl(isPassed, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }

        explicit Assert(const void* const pointer, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AsserterInstance::Get().AssertImpl(pointer != nullptr, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }

        explicit Assert(HRESULT hr, std::format_string<Args...> format = "", Args&&... args, const std::source_location& sourceLocation = std::source_location::current())
        {
            AsserterInstance::Get().AssertImpl(hr, sourceLocation, std::format(format, std::forward<Args>(args)...));
        }
    };

    template <typename... Args>
    Assert(bool, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(const void* const, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

    template <typename... Args>
    Assert(HRESULT, std::format_string<Args...>, Args&&...) -> Assert<Args...>;

} // namespace benzin

#if BENZIN_IS_DEBUG_BUILD
  #define BenzinAssert(condition, ...) ::benzin::Assert{ condition, __VA_ARGS__ }
#else
  #define BenzinAssert(condition, ...) (void)(condition) // The condition can be an expression
#endif

#define BenzinEnsure(condition, ...) ::benzin::Assert{ condition, __VA_ARGS__ }
