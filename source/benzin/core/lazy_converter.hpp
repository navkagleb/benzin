#pragma once

namespace benzin
{

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
