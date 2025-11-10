/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <cstddef> // for size_t
#include <map>     // for map
#include <utility> // for pair
#include <vector>  // for vector

////////////////
// Ordered_Map
////////////////

namespace credence {

template<typename Key, typename Value>
struct Ordered_Map
{
    void insert(Key const& key, Value const& value)
    {
        if (key_to_index.find(key) == key_to_index.end()) {
            data.emplace_back(key, value);
            key_to_index[key] = data.size() - 1;
        } else {
            data[key_to_index[key]].second = value;
        }
    }
    Value& operator[](Key const& key)
    {
        if (!contains(key)) {
            data.emplace_back(key, Value());
            key_to_index[key] = data.size() - 1;
        }
        return data[key_to_index[key]].second;
    }

    constexpr inline std::pair<Key, Value> first() { return data.front(); }

    constexpr inline std::pair<Key, Value> last() { return data.back(); }

    constexpr inline std::pair<Key, Value> prev()
    {
        return data.size() > 1 ? data.at(data.size() - 2) : data.back();
    }

    constexpr inline std::pair<Key, Value> at(std::size_t at)
    {
        return data.at(at);
    }

    constexpr inline std::size_t size() { return data.size(); }

    constexpr inline bool empty() { return data.empty(); }

    const Value& operator[](Key const& key) const
    {
        if (!contains(key)) {
            data.emplace_back(key, Value());
            key_to_index[key] = data.size() - 1;
        }
        return data[key_to_index.at(key)].second;
    }
    constexpr bool contains(Key const& key) const
    {
        return key_to_index.contains(key);
    }
    constexpr void clear()
    {
        data.clear();
        key_to_index.clear();
    }
    // Iteration in insertion order
    constexpr typename std::vector<std::pair<Key, Value>>::iterator begin()
    {
        return data.begin();
    }
    constexpr typename std::vector<std::pair<Key, Value>>::iterator end()
    {
        return data.end();
    }
    constexpr typename std::vector<std::pair<Key, Value>>::const_iterator
    begin() const
    {
        return data.begin();
    }
    constexpr typename std::vector<std::pair<Key, Value>>::const_iterator end()
        const
    {
        return data.end();
    }

  private:
    std::vector<std::pair<Key, Value>> data;
    std::map<Key, size_t> key_to_index;
};

} // namespace credence