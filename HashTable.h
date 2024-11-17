#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <mutex>

#include "Vector.h"
#include "ForwardList.h"
#include <shared_mutex>
using namespace std;

template <class Key, class Value>
class HashTable {
private:
    Vector<LinkedList<pair<Key, Value>>> table_;
    size_t size_;
    size_t capacity_;
    shared_mutex mutex_;

    size_t hash(const Key& key) const {
        return std::hash<Key>()(key) % capacity_;
    }

    double load_factor() const {
        return static_cast<double>(size_) / capacity_;
    }

    void rehash() {
        capacity_ *= 2;
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
        using ListIterator = typename ListType::Iterator;

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
            table_[index].push_front(keyValue);
            ++size_;
        }
        else {
            for (auto& [existing_key, existing_value] : table_[index]) {
                if (existing_key == keyValue.first) {
                    existing_value = keyValue.second;
                    return;
                }
            }
            table_[index].push_back(keyValue);
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
        size_t index = hash(key);

        if (table_[index].getSize() == 1) [[likely]] {
            const auto& [existing_key, _] = table_[index].front();
            return existing_key == key;
        }

        for (auto& [existing_key, existing_value] : table_[index]) {
            if (existing_key == key)
                return true;
        }
        return false;
    }

    void erase(const Key& key) {
        unique_lock lock(mutex_);
        size_t index = hash(key);
        if (table_[index].getSize() == 1) [[likely]] {
            table_[index].pop_front();
            --size_;
            return;
        }
        for (auto& [existing_key, existing_value] : table_[index]) {
            if (existing_key == key) {
                table_[index].remove(table_[index].index({existing_key, existing_value}));
                --size_;
                return;
            }
        }
    }

    [[nodiscard]] int getSize() const { return size_; }
};

#endif //HASHTABLE_H
