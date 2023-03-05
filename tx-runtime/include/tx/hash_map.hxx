#pragma once

#include "tx/hash.hxx"
#include "tx/memory.hxx"
#include "tx/utils.hxx"

#include <gsl/util>

#include <cassert>
#include <cstddef>
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
    typename KeyEqual = std::equal_to<Key>,
    typename SizeT = size_t>
class HashMap {
    static_assert(EMPTY_VALUE != TOMBSTONE_VALUE);

  public:
    using Entry = std::pair<const Key, T>;

    class Iterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using pointer = value_type*;
        using reference = value_type&;

        HashMap* table_ptr = nullptr;
        HashMap::Entry* ptr = nullptr;

        friend class HashMap;

        constexpr void advance() {
            while (ptr != std::next(table_ptr->data_ptr, table_ptr->capacity)
                   && is_entry_empty(*ptr)) {
                ++ptr;
            }
        }

      public:
        constexpr Iterator(HashMap* map, HashMap::Entry* entry_ptr)
                : table_ptr(map)
                , ptr(entry_ptr) {}

        [[nodiscard]] constexpr bool operator==(const Iterator& other
        ) const noexcept {
            return ptr == other.ptr;
        }

        [[nodiscard]] constexpr bool operator!=(const Iterator& other
        ) const noexcept {
            return ptr != other.ptr;
        }

        constexpr Iterator& operator++() noexcept {
            ++ptr;
            advance();
            return *this;
        }

        constexpr Iterator operator++(int) noexcept {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] constexpr Entry& operator*() noexcept { return *ptr; }

        [[nodiscard]] constexpr Entry* operator->() noexcept { return ptr; }
    };

    class ConstIterator {
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = Entry;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;

        const HashMap* table_ptr = nullptr;
        const HashMap::Entry* ptr = nullptr;

        friend class HashMap;

        constexpr void advance() {
            while (ptr != std::next(table_ptr->data_ptr, table_ptr->capacity)
                   && is_entry_empty(*ptr)) {
                ++ptr;
            }
        }

      public:
        constexpr ConstIterator(
            const HashMap* map,
            const HashMap::Entry* entry_ptr
        )
                : table_ptr(map)
                , ptr(entry_ptr) {}

        [[nodiscard]] constexpr bool operator==(const ConstIterator& other
        ) const noexcept {
            return ptr == other.ptr;
        }

        [[nodiscard]] constexpr bool operator!=(const ConstIterator& other
        ) const noexcept {
            return ptr != other.ptr;
        }

        constexpr ConstIterator& operator++() noexcept {
            ++ptr;
            advance();
            return *this;
        }

        constexpr ConstIterator operator++(int) noexcept {
            ConstIterator tmp = *this;
            ++(*this);
            return tmp;
        }

        [[nodiscard]] constexpr const Entry& operator*() const noexcept {
            return *ptr;
        }

        [[nodiscard]] constexpr const Entry* operator->() const noexcept {
            return ptr;
        }
    };

    using key_type = Key;
    using mapped_type = T;
    using value_type = Entry;
    using size_type = i32;
    using difference_type = std::ptrdiff_t;
    using iterator = Iterator;
    using const_iterator = ConstIterator;
    using pointer = value_type*;
    using const_pointer = const value_type*;
    using reference = value_type&;
    using const_reference = const value_type&;

  private:
    static constexpr float MAX_LOAD_FACTOR = 0.75;
    SizeT count = 0;  // count includes tombstones
    SizeT capacity = 0;
    gsl::owner<Entry*> data_ptr = nullptr;

  public:
    constexpr HashMap() noexcept = default;
    constexpr HashMap(const HashMap& other) noexcept = delete;
    constexpr HashMap(HashMap&& other) noexcept = delete;

    constexpr ~HashMap() noexcept {
        assert(count == 0);
        assert(capacity == 0);
        assert(data_ptr == nullptr);
    }

    constexpr void destroy(VM& tvm) noexcept {
        clear();
        if (data_ptr != nullptr) {
            free_array(tvm, data_ptr, capacity);
            data_ptr = nullptr;
        }
    }

    constexpr HashMap& operator=(const HashMap& other) noexcept = delete;
    constexpr HashMap& operator=(HashMap&& other) noexcept = delete;

    [[nodiscard]] static constexpr float max_load_factor() noexcept {
        return MAX_LOAD_FACTOR;
    }

    constexpr void clear() noexcept {
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

    template <typename... Args>
    constexpr bool set(VM& tvm, Key key, Args&&... args) noexcept {
        assert(!KeyEqual()(key, EMPTY_KEY) && "Key cannot be the empty key.");
        if (count + 1 > static_cast<SizeT>(
                static_cast<float>(capacity) * max_load_factor()
            )) {
            rehash_impl(tvm, grow_capacity(capacity));
        }
        Entry& entry = find_entry(key);
        bool is_new_key = is_entry_empty(entry);
        if (is_new_key && !is_entry_tombstone(entry)) { ++count; }
        auto* ptr = &entry;
        std::destroy_at(ptr);
        std::construct_at(ptr, key, std::forward<Args>(args)...);
        return is_new_key;
    }

    constexpr bool erase(const Key& key) noexcept {
        if (count == 0) { return false; }
        Entry& entry = find_entry(key);
        if (is_entry_empty(entry)) { return false; }
        auto* ptr = &entry;
        std::destroy_at(ptr);
        std::construct_at(ptr, EMPTY_KEY, TOMBSTONE_VALUE);
        return true;
    }

    constexpr void add_all_from(VM& tvm, const HashMap& from) noexcept {
        for (i32 i = 0; i < from.capacity; ++i) {
            Entry& entry = *std::next(from.data_ptr, i);
            if (!is_entry_empty(entry)) { set(tvm, entry.first, entry.second); }
        }
    }

    constexpr void rehash(VM& tvm, SizeT new_cap) noexcept {
        rehash_impl(tvm, power_of_2_ceil(new_cap));
    }

    template <typename F>
    [[nodiscard]] constexpr Entry* find_in_bucket(u32 hash, F func) noexcept {
        if (count == 0) { return nullptr; }
        auto& result = find_in_bucket_impl(hash, func);
        if (is_entry_empty(result)) { return nullptr; }
        return &result;
    }

    [[nodiscard]] constexpr Iterator begin() noexcept {
        Iterator iter(this, data_ptr);
        iter.advance();
        return iter;
    }

    [[nodiscard]] constexpr ConstIterator begin() const noexcept {
        return cbegin();
    }

    [[nodiscard]] constexpr ConstIterator cbegin() const noexcept {
        ConstIterator iter(this, data_ptr);
        iter.advance();
        return iter;
    }

    [[nodiscard]] constexpr Iterator end() noexcept {
        return Iterator(this, std::next(data_ptr, capacity));
    }

    [[nodiscard]] constexpr ConstIterator end() const noexcept {
        return cend();
    }

    [[nodiscard]] constexpr ConstIterator cend() const noexcept {
        return ConstIterator(this, std::next(data_ptr, capacity));
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
        assert(is_power_of_2(capacity));
        usize index = hash & gsl::narrow_cast<usize>(capacity - 1);
        Entry* tombstone = nullptr;
        while (true) {
            Entry& entry = *std::next(
                data_ptr,
                gsl::narrow_cast<gsl::index>(index)
            );
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
            index = (index + 1) & gsl::narrow_cast<usize>(capacity - 1);
        }
    }

    constexpr void rehash_impl(VM& tvm, SizeT new_cap) noexcept {
        assert(is_power_of_2(new_cap));
        auto* old_ptr = data_ptr;
        auto old_capacity = capacity;
        data_ptr = allocate<Entry>(tvm, new_cap);
        std::uninitialized_fill_n(
            data_ptr,
            new_cap,
            Entry(EMPTY_KEY, EMPTY_VALUE)
        );
        capacity = new_cap;
        count = 0;
        for (i32 i = 0; i < old_capacity; ++i) {
            Entry* src = std::next(old_ptr, i);
            if (is_entry_empty(*src)) { continue; }
            Entry* dest = &find_entry(src->first);
            std::destroy_at(dest);
            std::construct_at(dest, std::move(*src));
            ++count;
        }
        if (old_ptr != nullptr) {
            std::destroy_n(old_ptr, old_capacity);
            free_array<Entry>(tvm, old_ptr, old_capacity);
        }
    }
};

}  // namespace tx
