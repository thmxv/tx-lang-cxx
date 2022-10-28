#pragma once

#include "tx/hash.hxx"
#include "tx/memory.hxx"

#include <cassert>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

namespace tx {

template <
    typename Key,
    typename T,
    Key EMPTY_KEY,
    T EMPTY_VALUE,
    T TOMBSTONE_VALUE,
    typename Hash = Hash<Key>,
    typename KeyEqual = std::equal_to<Key> >
class HashMap {
    static_assert(EMPTY_VALUE != TOMBSTONE_VALUE);

  public:
    using Entry = std::pair<Key, T>;

  private:
    static constexpr float MAX_LOAD_FACTOR = 0.75;
    i32 count = 0;  // count includes tombstones
    i32 capacity = 0;
    gsl::owner<Entry*> data_ptr = nullptr;

  public:
    using value_type = std::pair<const Key, T>;
    using size_type = i32;

    constexpr HashMap() noexcept = default;
    constexpr HashMap(const HashMap& other) noexcept = delete;
    constexpr HashMap(HashMap&& other) noexcept = delete;

    constexpr ~HashMap() noexcept {
        assert(count == 0);
        assert(capacity == 0);
        assert(data_ptr == nullptr);
    }

    constexpr void destroy(VM& tvm) noexcept {
        std::destroy_n(data_ptr, capacity);
        if (data_ptr != nullptr) { free_array(tvm, data_ptr, capacity); }
        count = 0;
        capacity = 0;
        data_ptr = nullptr;
    }

    constexpr HashMap& operator=(const HashMap& other) noexcept = delete;
    constexpr HashMap& operator=(HashMap&& other) noexcept = delete;

    [[nodiscard]] static constexpr float max_load_factor() noexcept {
        return MAX_LOAD_FACTOR;
    }

    constexpr void clear() noexcept{
        std::destroy_n(data_ptr, capacity);
        count = 0;
        capacity = 0;
    }

    [[nodiscard]] constexpr T* get(const Key& key) noexcept {
        if (count == 0) { return nullptr; }
        Entry& entry = find_entry(key);
        if (is_entry_empty(entry)) { return nullptr; }
        return &entry.second;
    }

    // template <typename... Args>
    // constexpr bool set(VM& tvm, Key key, Args&&... args) noexcept {
    constexpr bool set(VM& tvm, Key key, T&& value) noexcept {
        assert(!KeyEqual()(key, EMPTY_KEY) && "Key cannot be the empty key.");
        if (count + 1 > static_cast<i32>(
                static_cast<float>(capacity) * max_load_factor()
            )) {
            rehash(tvm, grow_capacity(capacity));
        }
        Entry& entry = find_entry(key);
        bool is_new_key = is_entry_empty(entry);
        if (is_new_key && !is_entry_tombstone(entry)) { ++count; }
        entry.first = key;
        // entry.second = T(std::forward<Args>(args)...);
        // entry.second = std::move(value);
        entry.second = value;
        return is_new_key;
    }

    constexpr bool erase(const Key& key) noexcept {
        if (count == 0) { return false; }
        Entry& entry = find_entry(key);
        if (is_entry_empty(entry)) { return false; }
        entry.first = EMPTY_KEY;
        entry.second = TOMBSTONE_VALUE;
        return true;
    }

    constexpr void add_all_from(VM& tvm, const HashMap& from) noexcept {
        for (i32 i = 0; i < from.capacity; ++i) {
            Entry& entry = *std::next(from.data_ptr, i);
            if (!is_entry_empty(entry)) { set(tvm, entry.first, entry.second); }
        }
    }

    constexpr void rehash(VM& tvm, i32 new_cap) noexcept {
        // TODO: Make new_cap power of 2 and big enough
        Entry* entries = allocate<Entry>(tvm, new_cap);
        std::uninitialized_fill_n(
            entries,
            new_cap,
            Entry(EMPTY_KEY, EMPTY_VALUE)
        );
        count = 0;
        for (i32 i = 0; i < capacity; ++i) {
            Entry& entry = data_ptr[i];
            if (is_entry_empty(entry)) { continue; }
            Entry& dest = find_entry(entry.first);
            dest = std::move(entry);
            ++count;
        }
        if (data_ptr != nullptr) { free_array<Entry>(tvm, data_ptr, capacity); }
        data_ptr = entries;
        capacity = new_cap;
    }

    template <typename F>
    [[nodiscard]] constexpr Entry* find_in_bucket(u32 hash, F func) noexcept {
        if (count == 0) { return nullptr; }
        auto& result = find_in_bucket_impl(hash, func);
        if (is_entry_empty(result)) { return nullptr; }
        return &result;
    }

  private:
    [[nodiscard]] static constexpr bool is_entry_empty(const Entry& entry
    ) noexcept {
        return KeyEqual()(entry.first, EMPTY_KEY);
    }

    [[nodiscard]] static constexpr bool is_entry_tombstone(const Entry& entry
    ) noexcept {
        return entry.second == TOMBSTONE_VALUE;
    }

    [[nodiscard]] constexpr Entry& find_entry(Key key) noexcept {
        assert(!KeyEqual()(key, EMPTY_KEY) && "Key cannot be the empty key.");
        const u32 hash = Hash()(key);
        return find_in_bucket_impl(hash, [&key](Entry& entry) {
            return KeyEqual()(entry.first, key);
        });
    }

    template <typename F>
    [[nodiscard]] constexpr Entry&
    find_in_bucket_impl(u32 hash, F func) noexcept {
        assert(capacity > 0);
        u32 index = hash % static_cast<u32>(capacity);
        Entry* tombstone = nullptr;
        while (true) {
            Entry& entry = *std::next(data_ptr, static_cast<gsl::index>(index));
            if (is_entry_empty(entry)) {
                if (!is_entry_tombstone(entry)) {
                    // found the end of the bucket
                    return tombstone != nullptr ? *tombstone : entry;
                } else {
                    if (tombstone == nullptr) { tombstone = &entry; }
                }
            } else if (std::invoke(func, entry)) {
                // found the entry
                return entry;
            }
            index = (index + 1) % static_cast<u32>(capacity);
        }
    }
};

}  // namespace tx
