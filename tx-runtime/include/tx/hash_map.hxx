#pragma once

#include "tx/hash.hxx"
#include "tx/memory.hxx"

#include <cassert>
#include <functional>
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
  protected:
    static_assert(EMPTY_VALUE != TOMBSTONE_VALUE);

    using Entry = std::pair<Key, T>;

    static constexpr float MAX_LOAD_FACTOR = 0.75;

    // count includes tombstones
    i32 count = 0;
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
        // TODO call destructor on entries
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

    [[nodiscard]] constexpr std::optional<T&> get(const Key& key) noexcept {
        if (count == 0) { return std::nullopt; }
        const Entry& entry = find_entry(key);
        if (is_entry_empty(entry)) { return std::nullopt; }
        return entry.second;
    }

    template <typename... Args>
    constexpr bool set(VM& tvm, Key key, Args&&... args) noexcept {
        assert(!KeyEqual()(key, EMPTY_KEY) && "Key cannot be the empty key.");
        if (count + 1 > static_cast<i32>(
                static_cast<float>(capacity) * max_load_factor()
            )) {
            rehash(tvm, grow_capacity(capacity));
        }
        Entry& entry = find_entry(key);
        bool is_new_key = is_entry_empty(entry);
        if (is_new_key && !is_entry_tombstone(entry)) { ++count; }
        // TODO destroy previous and construct new
        entry.first = key;
        entry.second = T(std::forward<Args>(args)...);
        return is_new_key;
    }

    constexpr bool erase(const Key& key) noexcept {
        if (count == 0) { return false; }
        Entry& entry = find_entry(key);
        if (is_entry_empty(entry)) { return false; }
        // TODO call destructors
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
        for (i32 i = 0; i < new_cap; ++i) {
            // TODO: use contruct_at and copy constructor
            std::next(entries, i)->first = EMPTY_KEY;
            std::next(entries, i)->second = EMPTY_VALUE;
        }
        count = 0;
        for (i32 i = 0; i < capacity; ++i) {
            const Entry& entry = data_ptr[i];
            if (is_entry_empty(entry)) { continue; }
            Entry& dest = find_entry(entry.first);
            // TODO: use contruct_at and move constructor
            dest = entry;
            ++count;
        }
        if (data_ptr != nullptr) { free_array<Entry>(tvm, data_ptr, capacity); }
        data_ptr = entries;
        capacity = new_cap;
    }

  protected:
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
        u32 index = Hash()(key) % static_cast<u32>(capacity);
        Entry* tombstone = nullptr;
        while (true) {
            Entry& entry = *std::next(data_ptr, static_cast<gsl::index>(index));
            if (is_entry_empty(entry)) {
                if (!is_entry_tombstone(entry)) {
                    return tombstone != nullptr ? *tombstone : entry;
                } else {
                    if (tombstone == nullptr) { tombstone = &entry; }
                }
            } else if (KeyEqual()(entry.first, key)) {
                return entry;
            }
            index = (index + 1) % static_cast<u32>(capacity);
        }
    }
};

}  // namespace tx
