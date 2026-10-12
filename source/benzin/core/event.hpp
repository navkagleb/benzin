#pragma once

namespace benzin
{

    template <typename PublisherT, typename KeyT, typename CallbackT>
    class Event2;

    template <typename PublisherT, typename KeyT, typename ResultT, typename... Args>
    class Event2<PublisherT, KeyT, ResultT(Args...)>
    {
    public:
        friend PublisherT;

        using Callback = std::function<ResultT(Args...)>;

        struct Binding
        {
            KeyT m_Key;
            Callback m_Callback;

            Binding(KeyT key, Callback callback)
                : m_Key{ key }
                , m_Callback{ callback }
            {}

            template <typename T>
            Binding(KeyT key, T* target, ResultT(T::*funcPtr)(Args...))
                : m_Key{ key }
                , m_Callback{ [target, funcPtr](Args... args) { return (target->*funcPtr)(args...); } }
            {}
        };

        void Subscribe(const Binding& binding)
        {
            m_Container.m_Bindings[binding.m_Key] = binding.m_Callback;
        }

        void Unsubscribe(KeyT key)
        {
            m_Container.m_Bindings.erase(key);
        }

    private:
        // Hide bindings map from publisher (PublisherT)
        class BindingContainer
        {
        private:
            friend Event2;
            std::unordered_map<KeyT, Callback> m_Bindings;
        };

        BindingContainer m_Container;

        template <typename... CallArgs>
        void Raise(CallArgs&&... args)
        {
            for (auto& [key, callback] : m_Container.m_Bindings)
            {
                callback(std::forward<CallArgs>(args)...);
            }
        }
    };

}