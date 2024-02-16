#pragma once

namespace benzin
{

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
            ms_Instance = std::make_unique<T>(std::forward<decltype(args)>(args)...);
            return *ms_Instance;
        }

        static T& Get()
        {
            return *ms_Instance;
        }

    private:
        inline static std::unique_ptr<T> ms_Instance;
    };

} // namespace benzin
