#pragma once

#include <cmath>
#include <cstdint>
#include <ctime>

#include <array>
#include <bitset>
#include <chrono>
#include <concepts>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <future>
#include <ios>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <print>
#include <random>
#include <ranges>
#include <set>
#include <source_location>
#include <span>
#include <stack>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

using namespace std::literals::string_literals;
using namespace std::literals::string_view_literals;

namespace benzin
{

    template <typename T>
    size_t GetStdHash(const T& hashType)
    {
        return std::hash<T>{}(hashType);
    }

    template <typename... Fs>
    struct VisitorMatch : Fs...
    {
        using Fs::operator()...;
    };

    template <typename... Fs>
    auto MakeVisitorMatch(Fs... lambdas)
    {
        return VisitorMatch<Fs...>{ lambdas... };
    }

    template <typename UniquePtrT, typename... Args>
    __forceinline void MakeUniquePtr(UniquePtrT& outUniquePtr, Args&&... args)
    {
        using InnerType = std::decay_t<UniquePtrT>::element_type;
        outUniquePtr = std::make_unique<InnerType>(std::forward<Args>(args)...);
    }

    template <typename T>
    __forceinline auto ToSpan(const T* data, size_t count = 1)
    {
        return std::span{ data, count };
    }

    template <typename T>
    __forceinline auto ToSpan(const std::vector<T>& vector)
    {
        return std::span<const T>{ vector };
    }

    template <typename T>
    __forceinline auto ToSingleByteSpan(const T& data, size_t sizeInBytes = sizeof(T))
    {
        return std::span{ (const std::byte*)&data, sizeInBytes };
    }

}
#define BenzinDefineStdHashForType(HashType, HashTypeVariableName, HashFunctionImpl) \
    template <> \
    struct std::hash<HashType> \
    { \
        size_t operator()([[maybe_unused]] const HashType& HashTypeVariableName) const \
        HashFunctionImpl \
    } 

template <typename... Ts, typename... Fs>
constexpr decltype(auto) operator|(const std::variant<Ts...>& variant, const benzin::VisitorMatch<Fs...>& visitorMatch)
{
    return std::visit(visitorMatch, variant);
}
