/* SPDX-License-Identifier: MIT */
/* Copyright © 2022 Max Bachmann */

#pragma once
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <type_traits>
#include <vector>

namespace jaro_winkler {
namespace common {

/**
 * @defgroup Common Common
 * Common utilities shared among multiple functions
 * @{
 */

/* taken from https://stackoverflow.com/a/30766365/11335032 */
template <typename T>
struct is_iterator {
    static char test(...);

    template <typename U, typename = typename std::iterator_traits<U>::difference_type,
              typename = typename std::iterator_traits<U>::pointer,
              typename = typename std::iterator_traits<U>::reference,
              typename = typename std::iterator_traits<U>::value_type,
              typename = typename std::iterator_traits<U>::iterator_category>
    static long test(U&&);

    constexpr static bool value = std::is_same<decltype(test(std::declval<T>())), long>::value;
};

constexpr double result_cutoff(double result, double score_cutoff)
{
    return (result >= score_cutoff) ? result : 0;
}

template <typename T, typename U>
T ceildiv(T a, U divisor)
{
    return (T)(a / divisor) + (T)((a % divisor) != 0);
}

/**
 * @brief Finds the first mismatching pair of elements from two ranges:
 * one defined by [first1, last1) and another defined by [first2,last2).
 * Similar implementation to std::mismatch from C++14
 *
 * @param first1, last1	-	the first range of the elements
 * @param first2, last2	-	the second range of the elements
 *
 * @return std::pair with iterators to the first two non-equal elements.
 */
template <typename InputIt1, typename InputIt2>
std::pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1, InputIt2 first2,
                                       InputIt2 last2)
{
    while (first1 != last1 && first2 != last2 && *first1 == *first2) {
        ++first1;
        ++first2;
    }
    return std::pair<InputIt1, InputIt2>(first1, first2);
}

/**
 * Removes common prefix of two string views // todo
 */
template <typename InputIt1, typename InputIt2>
int64_t remove_common_prefix(InputIt1& first1, InputIt1 last1, InputIt2& first2, InputIt2 last2)
{
    int64_t prefix = static_cast<int64_t>(
        std::distance(first1, common::mismatch(first1, last1, first2, last2).first));
    first1 += prefix;
    first2 += prefix;
    return prefix;
}

struct BitvectorHashmap {
    struct MapElem {
        uint64_t key = 0;
        uint64_t value = 0;
    };

    BitvectorHashmap() : m_map()
    {}

    template <typename CharT>
    void insert(CharT key, int64_t pos)
    {
        insert_mask(key, 1ull << pos);
    }

    template <typename CharT>
    void insert_mask(CharT key, uint64_t mask)
    {
        int64_t i = lookup((uint64_t)key);
        m_map[i].key = key;
        m_map[i].value |= mask;
    }

    template <typename CharT>
    uint64_t get(CharT key) const
    {
        return m_map[lookup((uint64_t)key)].value;
    }

private:
    /**
     * lookup key inside the hasmap using a similar collision resolution
     * strategy to CPython and Ruby
     */
    int64_t lookup(uint64_t key) const
    {
        int64_t i = key % 128;

        if (!m_map[i].value || m_map[i].key == key) {
            return i;
        }

        int64_t perturb = key;
        while (true) {
            i = ((i * 5) + perturb + 1) % 128;
            if (!m_map[i].value || m_map[i].key == key) {
                return i;
            }

            perturb >>= 5;
        }
    }

    std::array<MapElem, 128> m_map;
};

struct PatternMatchVector {
    struct MapElem {
        uint64_t key = 0;
        uint64_t value = 0;
    };

    PatternMatchVector() : m_map(), m_extendedAscii()
    {}

    template <typename InputIt1>
    PatternMatchVector(InputIt1 first, InputIt1 last) : m_map(), m_extendedAscii()
    {
        insert(first, last);
    }

    template <typename InputIt1>
    void insert(InputIt1 first, InputIt1 last)
    {
        uint64_t mask = 1;
        for (int64_t i = 0; i < std::distance(first, last); ++i) {
            auto key = first[i];
            if (key >= 0 && key <= 255) {
                m_extendedAscii[key] |= mask;
            }
            else {
                m_map.insert_mask(key, mask);
            }
            mask <<= 1;
        }
    }

    template <typename CharT>
    void insert(CharT key, int64_t pos)
    {
        uint64_t mask = 1ull << pos;
        if (key >= 0 && key <= 255) {
            m_extendedAscii[key] |= mask;
        }
        else {
            m_map.insert_mask(key, mask);
        }
    }

    template <typename CharT>
    uint64_t get(CharT key) const
    {
        if (key >= 0 && key <= 255) {
            return m_extendedAscii[key];
        }
        else {
            return m_map.get(key);
        }
    }

private:
    BitvectorHashmap m_map;
    std::array<uint64_t, 256> m_extendedAscii;
};

struct BlockPatternMatchVector {
    BlockPatternMatchVector() : m_block_count(0)
    {}

    template <typename InputIt1>
    BlockPatternMatchVector(InputIt1 first, InputIt1 last) : m_block_count(0)
    {
        insert(first, last);
    }

    template <typename CharT>
    void insert(int64_t block, CharT key, int pos)
    {
        uint64_t mask = 1ull << pos;

        assert(block < m_block_count);
        if (key >= 0 && key <= 255) {
            m_extendedAscii[key * m_block_count + block] |= mask;
        }
        else {
            m_map[block].insert_mask(key, mask);
        }
    }

    template <typename InputIt1>
    void insert(InputIt1 first, InputIt1 last)
    {
        int64_t len = std::distance(first, last);
        m_block_count = ceildiv(len, 64);
        m_map.resize(m_block_count);
        m_extendedAscii.resize(m_block_count * 256);

        for (int64_t i = 0; i < len; ++i) {
            int64_t block = i / 64;
            int64_t pos = i % 64;
            insert(block, first[i], pos);
        }
    }

    template <typename CharT>
    uint64_t get(int64_t block, CharT key) const
    {
        assert(block < m_block_count);
        if (key >= 0 && key <= 255) {
            return m_extendedAscii[key * m_block_count + block];
        }
        else {
            return m_map[block].get(key);
        }
    }

private:
    std::vector<BitvectorHashmap> m_map;
    std::vector<uint64_t> m_extendedAscii;
    int64_t m_block_count;
};

/**@}*/

} // namespace common
} // namespace jaro_winkler
