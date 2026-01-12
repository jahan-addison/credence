/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include <cstddef> // for size_t
#include <map>     // for map
#include <utility> // for pair
#include <vector>  // for vector

/****************************************************************************
 *
 * Ordered Map
 *
 * A map that preserves insertion order, unlike std::map which orders by key.
 * Used throughout translation phases where declaration order matters, such as
 * maintaining the order of global variables and functions.
 *
 * Example - preserving declaration order:
 *
 *   first 10;
 *   second 20;
 *   third 30;
 *
 * These globals must be emitted in assembly in the same order they were
 * declared. An ordered map ensures:
 *
 *   .data
 *   first:  .quad 10
 *   second: .quad 20
 *   third:  .quad 30
 *
 *****************************************************************************/

namespace credence {

template<typename Key, typename Value>
class Ordered_Map
{
  public:
    using Entry = std::pair<Key, Value>;

  public:
    constexpr explicit Ordered_Map() = default;

  public:
    Ordered_Map(Ordered_Map const&) = default;
    Ordered_Map& operator=(Ordered_Map const&) = default;
    Ordered_Map(Ordered_Map&& other) = default;

  public:
    constexpr void insert(Key const& key, Value const& value)
    {
        if (key_to_index.find(key) == key_to_index.end()) {
            data.emplace_back(key, value);
            key_to_index[key] = data.size() - 1;
        } else {
            data[key_to_index[key]].second = value;
        }
    }
    constexpr Value& operator[](Key const& key)
    {
        if (!contains(key)) {
            data.emplace_back(key, Value());
            key_to_index[key] = data.size() - 1;
        }
        return data[key_to_index[key]].second;
    }

    constexpr Value& operator[](Key const& key) const
    {
        if (!contains(key)) {
            data.emplace_back(key, Value());
            key_to_index[key] = data.size() - 1;
        }
        return data[key_to_index.at(key)].second;
    }

    constexpr inline Entry first() { return data.front(); }
    constexpr inline Entry last() { return data.back(); }
    constexpr inline Entry prev()
    {
        return data.size() > 1 ? data.at(data.size() - 2) : data.back();
    }
    constexpr inline Value at(Key const& key)
    {
        return data[key_to_index[key]].second;
    }
    constexpr inline std::size_t size() { return data.size(); }
    constexpr inline bool empty() { return data.empty(); }

    constexpr bool contains(Key const& key) const
    {
        return key_to_index.contains(key);
    }
    constexpr void clear()
    {
        data.clear();
        key_to_index.clear();
    }
    constexpr typename std::vector<Entry>::iterator begin()
    {
        return data.begin();
    }
    constexpr typename std::vector<Entry>::reverse_iterator rbegin()
    {
        return data.rbegin();
    }
    constexpr typename std::vector<Entry>::iterator end() { return data.end(); }
    constexpr typename std::vector<Entry>::reverse_iterator rend()
    {
        return data.rend();
    }
    constexpr typename std::vector<Entry>::const_iterator begin() const
    {
        return data.begin();
    }
    constexpr typename std::vector<Entry>::const_reverse_iterator rbegin() const
    {
        return data.rbegin();
    }
    constexpr typename std::vector<Entry>::const_iterator end() const
    {
        return data.end();
    }
    constexpr typename std::vector<Entry>::const_reverse_iterator rend() const
    {
        return data.rend();
    }

  private:
    std::vector<Entry> data;
    std::map<Key, std::size_t> key_to_index;
};

} // namespace credence