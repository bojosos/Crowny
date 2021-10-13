#pragma once

#include <algorithm>
#include <functional>
#include <memory>
#include <random>
#include <utility>

#include <array>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <cstring>

#include <iostream>
#include <istream>

#include <filesystem>
#include <fstream>

namespace Crowny
{

    using Path = std::filesystem::path;

    namespace fs = std::filesystem;

    struct HashPath
    {
        std::size_t operator()(const Path& path) const { return std::hash<std::string>()(path.string()); }
    };

    template <typename T> using HashType = std::hash<T>;

    template <typename T> using Deque = std::deque<T>;

    template <typename T> using Vector = std::vector<T>;

    template <typename T, std::size_t N> using Array = std::array<T, N>;

    template <typename K, typename V, typename P = std::less<K>> using Map = std::map<K, V, P>;

    template <typename T> using List = std::list<T>;

    template <typename T, typename P = std::less<T>> using Set = std::set<T, P>;

    template <typename K, typename V, typename P = std::less<K>> using Multimap = std::multimap<K, V, P>;

    template <typename K, typename V, typename H = HashType<K>, typename C = std::equal_to<K>>
    using UnorderedMap = std::unordered_map<K, V, H, C>;

    template <typename T, typename H = HashType<T>, typename C = std::equal_to<T>>
    using UnorderedSet = std::unordered_set<T, H, C>;

    template <typename K, typename V, typename H = HashType<K>, typename C = std::equal_to<K>>
    using UnorderedMultimap = std::unordered_multimap<K, V, H, C>;

    template <typename T> using Stack = std::stack<T>;

    template <typename T> using Queue = std::queue<T>;

    using String = std::string;

    using U16String = std::u16string;

    using U32String = std::u32string;

    using Stringstream = std::stringstream;

    template <typename T> using Scope = std::unique_ptr<T>;

    /**
     * @brief Creates a unique pointer.
     *
     * @tparam Smart pointer type.
     */
    template <typename T, typename... Args> constexpr Scope<T> CreateScope(Args&&... args)
    {
        return std::make_unique<T>(std::forward<Args>(args)...);
    }

    template <typename T> using Ref = std::shared_ptr<T>;

    /**
     * @brief Creates a shared pointer.
     *
     * @tparam Smart pointer type.
     */
    template <typename T, typename... Args> constexpr Ref<T> CreateRef(Args&&... args)
    {
        return std::make_shared<T>(std::forward<Args>(args)...);
    }

} // namespace Crowny
