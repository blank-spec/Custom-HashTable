#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <mutex>

#include "Vector.h"
#include "ForwardList.h"
#include <shared_mutex>
using namespace std;

namespace std {
    struct HashCombiner {
        template<typename T>
        size_t operator()(size_t seed, const T& v) const {
            seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    template<typename T1, typename T2>
    struct hash<pair<T1, T2>> {
        size_t operator()(const pair<T1, T2>& p) const {
            size_t seed = 0;
            HashCombiner combiner;
            seed = combiner(seed, p.first);
            seed = combiner(seed, p.second);
            return seed;
        }
    };
}

template <typename Key, typename Value, typename Hash = std::hash<Key>>
class HashTable {
private:
    struct HashCombiner {
        template<typename T>
        size_t operator()(size_t seed, const T& v) const {
            seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    template<typename T1, typename T2>
    struct hash<pair<T1, T2>> {
        size_t operator()(const pair<T1, T2>& p) const {
            size_t seed = 0;
            HashCombiner combiner;
            seed = combiner(seed, p.first);
            seed = combiner(seed, p.second);
            return seed;
        }
    };
    
    Vector<LinkedList<pair<Key, Value>>> table_;
    size_t size_;
    size_t capacity_;
    shared_mutex mutex_;
    Hash hasher_;

    size_t hash(const Key& key) const {
        return hasher_(key) % capacity_;
    }

    [[nodiscard]] double load_factor() const {
        return static_cast<double>(size_) / capacity_;
    }

    void rehash() {
        capacity_ *= 1.5;
        Vector<LinkedList<pair<Key, Value>>> newTable(capacity_, LinkedList<pair<Key, Value>>());

        for (const auto& bucket : table_) {
            for (const auto& [key, value] : bucket) {
                size_t index = std::hash<Key>()(key) % capacity_;
                newTable[index].push_back(make_pair(key, value));
            }
        }

        table_ = move(newTable);
    }

public:
    template <bool IsConst>
    class HashTableIterator {
    private:
        using ListType = std::conditional_t<IsConst, const LinkedList<std::pair<Key, Value>>, LinkedList<std::pair<Key, Value>>>;
        using ListIterator = typename ListType::iterator;

        HashTable* table;
        size_t bucketIndex;
        ListIterator current;

        void moveToNextValidBucket() {
            while (bucketIndex < table->capacity_ && current == table->table_[bucketIndex].end()) {
                ++bucketIndex;
                if (bucketIndex < table->capacity_) {
                    current = table->table_[bucketIndex].begin();
                }
            }
        }

    public:
        using pointerType = std::conditional_t<IsConst, const std::pair<Key, Value>*, std::pair<Key, Value>*>;
        using referenceType = std::conditional_t<IsConst, const std::pair<Key, Value>&, std::pair<Key, Value>&>;
        using iterator_category = std::forward_iterator_tag;
        using difference_type = std::ptrdiff_t;
        using value_type = std::pair<Key, Value>;

        HashTableIterator(HashTable* table, size_t bucketIndex = 0)
            : table(table), bucketIndex(bucketIndex),
              current(bucketIndex < table->capacity_ ? table->table_[bucketIndex].begin() : ListIterator()) {
            moveToNextValidBucket();
        }

        HashTableIterator(HashTable* table, size_t bucketIndex, ListIterator current)
        : table(table), bucketIndex(bucketIndex), current(current) {}

        referenceType operator*() const { return *current; }
        pointerType operator->() const { return &(*current); }

        HashTableIterator& operator++() {
            ++current;
            moveToNextValidBucket();
            return *this;
        }

        HashTableIterator operator++(int) {
            HashTableIterator temp = *this;
            ++(*this);
            return temp;
        }

        bool operator==(const HashTableIterator& other) const {
            return table == other.table && bucketIndex == other.bucketIndex && current == other.current;
        }

        bool operator!=(const HashTableIterator& other) const {
            return !(*this == other);
        }

    };

    using iterator = HashTableIterator<false>;
    using const_iterator = HashTableIterator<true>;

    iterator begin() {
        return { this, 0 };
    }

    iterator end() {
        return { this, capacity_ };
    }

    const_iterator cbegin() const {
        return { this, 0 };
    }

    const_iterator cend() const {
        return { this, capacity_ };
    }

    const_iterator begin() const {
        return { this, 0 };
    }

    const_iterator end() const {
        return { this, capacity_ };
    }

    HashTable(size_t initial_capacity)
        : table_(initial_capacity, LinkedList<pair<Key, Value>>()), size_(0), capacity_(initial_capacity) {}

    HashTable()
        : table_(5, LinkedList<pair<Key, Value>>()), size_(0), capacity_(5) {}

    HashTable(std::initializer_list<pair<Key, Value>> init) : HashTable() {
        for (auto it = init.begin(); it != init.end(); ++it) {
            insert(*it);
        }
    }

    HashTable(const HashTable& other) noexcept : table_(other), size_(other.size_), capacity_(other.capacity_) {}

    HashTable(HashTable&& other) noexcept {
        table_ = move(other.table_);
        size_ = other.size_;
        capacity_ = other.capacity_;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    HashTable& operator=(const HashTable& other) noexcept {
        if (this != &other) {
            table_ = other.table_;
            size_ = other.size_;
            capacity_ = other.capacity_;
        }
        return *this;
    }

    HashTable& operator=(HashTable&& other) noexcept {
        if (this != &other) {
            table_ = move(other.table_);
            size_ = other.size_;
            capacity_ = other.capacity_;

            other.size_ = 0;
            other.capacity_ = 0;
        }
        return *this;
    }

    void insert(const pair<Key, Value>& keyValue) {
        unique_lock lock(mutex_);
        if (load_factor() > 0.7)
            rehash();

        size_t index = hash(keyValue.first);

        if (table_[index].empty()) [[likely]] {
            table_[index].push_front(move(keyValue));
            ++size_;
        }
        else {
            for (auto& [existing_key, existing_value] : table_[index]) {
                if (existing_key == keyValue.first) {
                    existing_value = move(keyValue.second);
                    return;
                }
            }
            table_[index].push_back(move(keyValue));
            ++size_;
        }
    }

    Value& operator[](const Key& key) {
        std::unique_lock write_lock(mutex_);
        auto& bucket = table_[hash(key)];

        for (auto& [existing_key, existing_value] : bucket) {
            if (existing_key == key) {
                return existing_value;
            }
        }

        bucket.push_back({key, Value()});
        return bucket.back().second;
    }

    bool contains(const Key& key) const {
        shared_lock read_lock(mutex_);
        size_t index = hash(key);
        for (const auto& [existing_key, _] : table_[index]) {
            if (existing_key == key)
                return true;
        }
        return false;
    }

    void erase(const Key& key) {
        size_t index = hash(key);
        for (auto it = table_[index].begin(); it != table_[index].end(); ++it) {
            if (it->first == key) {
                unique_lock lock(mutex_);
                table_[index].erase(it);
                --size_;
                return;
            }
        }
    }

    template <typename... Args>
    void emplace(Args&&... args) {
        insert(std::make_pair(std::forward<Args>(args)...));
    }

    iterator find(const Key& key) {
        size_t index = hash(key);
        for (auto it = table_[index].begin(); it != table_[index].end(); ++it) {
            if (it->first == key)
                return iterator(this, index, it);
        }
        return end();
    }

    void swap(HashTable& other) noexcept {
        std::swap(table_, other.table_);
        std::swap(size_, other.size_);
        std::swap(capacity_, other.capacity_);
        //std::swap(mutex_, other.mutex_);
        std::swap(hasher_, other.hasher_);
    }

    [[nodiscard]] int getSize() const { return size_; }
};

#endif //HASHTABLE_H
