#ifndef HASHMAP_HASH_MAP_H
#define HASHMAP_HASH_MAP_H

#include <memory>
#include <vector>
#include <algorithm>

/*
 * Early insert coalesced hashing with added cellar space
 */

template<class KeyType, class ValueType, class Hash = std::hash<KeyType> >
class HashMap {
public:
    using Element = std::pair<KeyType, ValueType>;
    using IteratorElement = std::pair<const KeyType, ValueType>;

    class iterator {
    public:
        iterator() = default;

        iterator(size_t index, HashMap *hashmap) : hashmap_(hashmap), index_(index) {
        }

        IteratorElement &operator*() const {
            if (index_ >= hashmap_->len_)
                throw std::out_of_range("dereference for non-existent element");
            return reinterpret_cast<IteratorElement &>(hashmap_->data_[index_]);
        }

        IteratorElement *operator->() const {
            if (index_ >= hashmap_->len_)
                throw std::out_of_range("member access for non-existent element");
            return reinterpret_cast<IteratorElement *>(&hashmap_->data_[index_]);
        }

        iterator &operator++() {
            ++index_;
            while (index_ < hashmap_->len_ && (!hashmap_->occupied_[index_] || hashmap_->erased_[index_])) {
                ++index_;
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++index_;
            while (index_ < hashmap_->len_ && (!hashmap_->occupied_[index_] || hashmap_->erased_[index_])) {
                ++index_;
            }
            return tmp;
        }

        bool operator==(const iterator &other) {
            return (index_ == other.index_ && hashmap_ == other.hashmap_);
        }

        bool operator!=(const iterator &other) {
            return (index_ != other.index_ || hashmap_ != other.hashmap_);
        }

    private:
        HashMap *hashmap_ = nullptr;
        size_t index_ = 0;
    };

    class const_iterator {
    public:
        const_iterator() = default;

        const_iterator(size_t index, const HashMap *hashmap) : hashmap_(hashmap), index_(index) {
        }

        const IteratorElement &operator*() const {
            if (index_ >= hashmap_->len_)
                throw std::out_of_range("dereference for non-existent element");
            return reinterpret_cast<const IteratorElement &>(hashmap_->data_[index_]);
        }

        const IteratorElement *operator->() const {
            if (index_ >= hashmap_->len_)
                throw std::out_of_range("member access for non-existent element");
            return reinterpret_cast<const IteratorElement *>(&(hashmap_->data_[index_]));
        }

        const_iterator &operator++() {
            ++index_;
            while (index_ < hashmap_->len_ && (!hashmap_->occupied_[index_] || hashmap_->erased_[index_])) {
                ++index_;
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++index_;
            while (index_ < hashmap_->len_ && (!hashmap_->occupied_[index_] || hashmap_->erased_[index_])) {
                ++index_;
            }
            return tmp;
        }

        bool operator==(const const_iterator &other) {
            return (index_ == other.index_ && hashmap_ == other.hashmap_);
        }

        bool operator!=(const const_iterator &other) {
            return (index_ != other.index_ || hashmap_ != other.hashmap_);
        }

    private:
        const HashMap *hashmap_ = nullptr;
        size_t index_ = 0;
    };


    explicit HashMap(Hash hasher = Hash()) : hasher_(hasher) {
    }

    template<class ForwardIterator>
    HashMap(ForwardIterator begin, ForwardIterator end, Hash hasher = Hash()): hasher_(hasher) {
        while (begin != end) {
            insert(*begin);
            ++begin;
        }
    }

    HashMap(std::initializer_list<Element> initializer_list, Hash hasher = Hash()) :
            hasher_(hasher) {
        for (const auto &element: initializer_list) {
            insert(element);
        }
    }

    iterator begin() {
        size_t index = 0;
        while (index < len_ && (!occupied_[index] || erased_[index])) {
            ++index;
        }
        return iterator(index, this);
    }

    const_iterator begin() const {
        size_t index = 0;
        while (index < len_ && (!occupied_[index] || erased_[index])) {
            ++index;
        }
        return const_iterator(index, this);
    }

    iterator end() {
        return iterator(len_, this);
    }

    const_iterator end() const {
        return const_iterator(len_, this);
    }

    void insert(const Element &element) {
        if (occupied_count_ >= len_ * MAX_LOAD_FACTOR) {
            resize();
        }
        size_t index = hasher_(element.first) % address_len_;
        size_t start = index;
        while (occupied_[index]) {
            if (data_[index].first == element.first) {
                if (erased_[index]) {
                    data_[index].second = element.second;
                    ++size_;
                    erased_[index] = false;
                }
                return;
            }
            if (next_[index] == NULL_INDEX) {
                index = get_last_empty();
            } else {
                index = next_[index];
            }
        }
        data_[index] = element;
        occupied_[index] = true;
        ++size_;
        ++occupied_count_;
        if (start != index) {
            next_[index] = next_[start];
            next_[start] = index;
        }
    }

    void erase(const KeyType &key) {
        if (len_ == 0) return;
        size_t index = hasher_(key) % address_len_;
        while (index != NULL_INDEX) {
            if (!occupied_[index]) return;
            if (data_[index].first == key) {
                if (!erased_[index]) {
                    erased_[index] = true;
                    --size_;
                }
                return;
            }
            index = next_[index];
        }
    }

    iterator find(const KeyType &key) {
        if (len_ == 0) return end();
        size_t index = hasher_(key) % address_len_;
        while (index != NULL_INDEX) {
            if (!occupied_[index]) return end();
            if (data_[index].first == key) {
                if (erased_[index]) return end();
                else return iterator(index, this);
            }
            index = next_[index];
        }
        return end();
    }

    const_iterator find(const KeyType &key) const {
        if (len_ == 0) return end();
        size_t index = hasher_(key) % address_len_;
        while (index != NULL_INDEX) {
            if (!occupied_[index]) return end();
            if (data_[index].first == key) {
                if (erased_[index]) return end();
                else return const_iterator(index, this);
            }
            index = next_[index];
        }
        return end();
    }

    ValueType &operator[](const KeyType &key) {
        auto it = find(key);
        if (it == end()) {
            insert(std::make_pair(key, ValueType()));
            it = find(key);
        }
        return it->second;
    }

    const ValueType &at(const KeyType &key) const {
        auto it = find(key);
        if (it == end()) {
            throw std::out_of_range("non-existent element");
        }
        return it->second;
    }

    void clear() {
        len_ = 0;
        address_len_ = 0;
        size_ = 0;
        occupied_count_ = 0;
        last_empty_ = 0;
        data_.clear();
        next_.clear();
        occupied_.clear();
        erased_.clear();
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return (size_ == 0);
    }

    Hash hash_function() const {
        return hasher_;
    }

private:
    constexpr static const float MAX_LOAD_FACTOR = 0.8;
    constexpr static const float ADDRESS_FACTOR = 0.86;
    constexpr static const size_t NULL_INDEX = (size_t) (-1);

    Hash hasher_;

    size_t len_ = 0;
    size_t address_len_ = 0;
    size_t size_ = 0;
    size_t occupied_count_ = 0;
    size_t last_empty_ = 0;

    std::vector<Element> data_;
    std::vector<size_t> next_;
    std::vector<bool> occupied_;
    std::vector<bool> erased_;

    size_t get_last_empty() {
        while (occupied_[last_empty_]) {
            --last_empty_;
        }
        return last_empty_;
    }

    void resize() {
        std::vector<Element> elements;
        for (size_t i = 0; i < len_; ++i) {
            if (occupied_[i] && !erased_[i]) {
                elements.push_back(data_[i]);
            }
        }
        std::fill(next_.begin(), next_.end(), NULL_INDEX);
        std::fill(occupied_.begin(), occupied_.end(), false);
        std::fill(erased_.begin(), erased_.end(), false);
        len_ = 2 * len_ + 7;
        address_len_ = std::max(1, static_cast<int>(len_ * ADDRESS_FACTOR));
        data_.resize(len_);
        occupied_.resize(len_, false);
        erased_.resize(len_, false);
        next_.resize(len_, NULL_INDEX);
        size_ = 0;
        occupied_count_ = 0;
        last_empty_ = len_ - 1;
        for (const auto &element: elements) {
            insert(element);
        }
    }
};

#endif //HASHMAP_HASH_MAP_H
