#ifndef JVN_ROBIN_HOOD_MAP_
#define JVN_ROBIN_HOOD_MAP_

#include "hash.h"
#include <utility>
#include <cstring>
#include <optional>

namespace jvn
{
    JVN_PRAGMA_PACK_PUSH(1)
    template <class Kt, class Vt>
    struct hash_bucket {
        // Distance from ideal hash position
        uint8_t id;
        std::pair<Kt, Vt> key_value_pair;
    };
    JVN_PRAGMA_PACK_POP()

    template <class Kt, class Vt, 
            class Hasher = hash<Kt>, 
            class KeyEq = std::equal_to<Kt>, 
            class Alloc = std::allocator<hash_bucket<Kt, Vt>>>
        class unordered_map
    {
    public:
        using hasher                = Hasher;
        using key_equal             = KeyEq;
        using allocator_type        = Alloc;
        using size_type             = typename Alloc::size_type;
        using difference_type       = typename Alloc::difference_type;
        using key_type              = Kt;
        using mapped_type           = Vt;
        using value_type            = std::pair<const key_type, mapped_type>;
        using pointer               = value_type*;
        using reference             = value_type&;

        using bucket_type           = hash_bucket<key_type, mapped_type>;
    private:
        using m_value_type          = std::pair<key_type, mapped_type>;
    public:

        class Iter
        {
        public:
            Iter(const Iter&)               = default;
            ~Iter()                         = default;
            Iter& operator=(const Iter&)    = default;

            inline friend constexpr bool operator==(const Iter& lhs, const Iter& rhs) noexcept { return lhs.m_bucket_ptr == rhs.m_bucket_ptr; }
            inline friend constexpr bool operator!=(const Iter& lhs, const Iter& rhs) noexcept { return !(lhs == rhs); }
            inline Iter& operator++() {
                while ((++m_bucket_ptr)->id == uint8_t(-1));
                return *this;
            }
            inline pointer operator->() const { return reinterpret_cast<pointer>(&(m_bucket_ptr->key_value_pair)); }
            inline reference operator*() const { return reinterpret_cast<reference>(m_bucket_ptr->key_value_pair); }
        private:
            friend class unordered_map;
            bucket_type* m_bucket_ptr;
            
            Iter(bucket_type* bucket_ptr): m_bucket_ptr(bucket_ptr)
            {   
                if (m_bucket_ptr->id == uint8_t(-1))
                    operator++();
            }
        };

        friend class Iter;
        using iterator              = Iter;

        unordered_map()
            :LOAD_FACTOR(0.8f),
            GROWTH_FACTOR(2),
            m_allocator(std::nullopt),
            m_capacity(64),
            m_size(0),
            m_max_elems(size_type(float(m_capacity) * LOAD_FACTOR))
        { initilizeBucket(); }

        unordered_map(allocator_type& allocator, size_type inital_capacity = 64, float load_factor = 0.8f, size_type growth_factor = 2)
            :LOAD_FACTOR(load_factor),
            GROWTH_FACTOR(closestPowerOfTwo(growth_factor)),
            m_allocator(std::ref(allocator)),
            m_capacity(closestPowerOfTwo(inital_capacity)),
            m_size(0),
            m_max_elems(size_type(float(m_capacity) * LOAD_FACTOR))
        { initilizeBucket(); }

        ~unordered_map() {
            for (bucket_type* iter = m_bucket; iter != end(); ++iter)
                if (iter->id != uint8_t(-1))
                    iter->key_value_pair.~m_value_type();

            m_allocator->get().deallocate(m_bucket, m_capacity + 1);
        }

        template <class KeyTy>
        inline mapped_type& operator[](KeyTy&& key) {
            return insert(value_type(std::forward<KeyTy>(key), mapped_type())).first->second;
        }

        template <class... Valtys>
        inline std::pair<iterator, bool> emplace(Valtys&&... vals) {
            return insert(value_type(std::forward<Valtys>(vals)...));
        }

        std::pair<iterator, bool> insert(m_value_type key_value_pair) {
            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key_value_pair.first);
            while (true) {
                // Empty slot found
                if (iter->id == uint8_t(-1)) {
                    iter->id = id;
                    ::new (&(iter->key_value_pair)) auto(std::move(key_value_pair));
                    break;
                }

                // Rich found
                if (iter->id < id) {
                    insertFrom(advanceIter(iter), iter->id + 1, std::move(iter->key_value_pair));
                    iter->id = id;
                    iter->key_value_pair = std::move(key_value_pair);

                    break;
                } 

                // Key found
                if (iter->id == id && JVN_UNLIKELY(m_key_equal(key_value_pair.first, iter->key_value_pair.first)))
                    return std::pair<iterator, bool>(iterator(iter), false);

                ++id;
                iter = advanceIter(iter);
            }

            if (JVN_LIKELY(++m_size != m_max_elems))
                return std::pair<iterator, bool>(iterator(iter), true);
            
            key_type key = iter->key_value_pair.first;
            grow();
            return std::pair<iterator, bool>(find(key), true);
        }


        template <class KeyTy>
        size_type erase(KeyTy&& key) noexcept {
            bucket_type* iter = find(std::forward<KeyTy>(key)).m_bucket_ptr;
            if (iter == m_bucket_end)
                return size_type(0);

            // Traverse the bucket and swap elements with previous until
            // an empty slot is found or an element with the 0 hash distance
            bucket_type* prev_iter = std::exchange(iter, advanceIter(iter));
            while (iter->id != uint8_t(0) && iter->id != uint8_t(-1)) {
                using std::swap;
                prev_iter->id = iter->id - 1;
                swap(prev_iter->key_value_pair, prev_iter->key_value_pair);

                prev_iter = std::exchange(iter, advanceIter(iter));
            }

            prev_iter->id = uint8_t(-1);
            prev_iter->key_value_pair.~m_value_type();
            --m_size;

            return size_type(1);
        }

        template <class KeyTy>
        iterator find(KeyTy&& key) const noexcept {
            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key);
            while (true) {
                // TODO: Check performance differnece of the addition of JVN_LIKELY
                // Key found
                if (iter->id == id && m_key_equal(iter->key_value_pair.first, key))
                    return iterator(iter);

                // Key not found
                if (JVN_UNLIKELY(iter->id == uint8_t(-1) || iter->id < id))
                    return end();

                ++id;
                iter = advanceIter(iter);
            }
        }

        inline size_type size() const noexcept { return m_size; }
        inline bool empty() const noexcept { return !m_size; }


        inline iterator begin() const noexcept { return iterator(m_bucket); }
        inline iterator end() const noexcept { return iterator(m_bucket_end); }

    private:
        float LOAD_FACTOR;
        size_type GROWTH_FACTOR;

        // TODO: Change to pointer ownership
        std::optional<std::reference_wrapper<allocator_type>> m_allocator;
        hasher m_hasher;
        key_equal m_key_equal;

        bucket_type* m_bucket;
        bucket_type* m_bucket_end;
        size_type m_capacity, m_size;
        // The number of elements that triggers grow()
        size_type m_max_elems;

        // Since m_capacity is always a power of two it's decremented value is all ones binary.
        // It can be used for fast trimming of the top bits of the hash, since % is a slow operation.
        template <class KeyTy>
        inline size_type hashAndTrim(KeyTy&& key) const noexcept { return m_hasher(std::forward<KeyTy>(key)) & (m_capacity - 1); }

        inline bucket_type* advanceIter(bucket_type* iter) const noexcept {
            if (JVN_UNLIKELY(++iter == m_bucket_end))
                    return m_bucket;

            return iter;
        }

        // Re-insert swapped rich element
        inline void insertFrom(bucket_type* iter, uint8_t id, m_value_type&& key_value_pair) {
            while (iter->id != uint8_t(-1)) {
                // Rich found
                if (iter->id < id) {
                    using std::swap;
                    swap(iter->id, id);
                    swap(iter->key_value_pair, key_value_pair);
                } 

                ++id;
                iter = advanceIter(iter);
            }

            iter->id = id;
            ::new (&(iter->key_value_pair)) auto(std::move(key_value_pair));
        }

        // TODO: Make exception safe
        void grow() {
            size_type prev_capacity = m_capacity;
            bucket_type* prev_bucket = m_bucket,
                    *prev_bucket_end = m_bucket_end;

            // Grow the bucket
            m_capacity *= GROWTH_FACTOR;
            m_max_elems = size_type(float(m_capacity) * LOAD_FACTOR);
            m_size = 0;
            initilizeBucket();

            // Rehash and insert
            for (auto iter = prev_bucket; iter != prev_bucket_end; ++iter)
                if (iter->id != uint8_t(-1))
                    insert(std::move(iter->key_value_pair));

            m_allocator->get().deallocate(prev_bucket, prev_capacity + 1);
        }


        void initilizeBucket() {
            // +1 is for the differantiation of the end() iterator
            m_bucket = m_allocator->get().allocate(m_capacity + 1);
            m_bucket_end = m_bucket + m_capacity;
            for (bucket_type* iter = m_bucket; iter != m_bucket_end; ++iter)
                iter->id = uint8_t(-1);
                
            //Element at the end must have a non -1u info value
            m_bucket[m_capacity].id = uint8_t(0);
        }

        // Returns the first equal or bigger power of two. The return value is always greater than 1.
        static size_type closestPowerOfTwo(size_type num) noexcept {
            if (num <= 2)
                return 2u;

            num--;
            num |= num >> 1;
            num |= num >> 2;
            num |= num >> 4;
            num |= num >> 8;
            num |= num >> 16;
            num++;

            return num;
        }
    };

} // namespace jvn

#endif