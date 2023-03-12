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
        using value_type            = std::pair<key_type, mapped_type>;
        using pointer               = value_type*;
        using reference             = value_type&;
        using const_reference       = const value_type&;
        using bucket_type           = hash_bucket<key_type, mapped_type>;
    public:

        class Iter
        {
        public:
            Iter(bucket_type* bucket_ptr): m_bucket_ptr(bucket_ptr)
            {   
                if (m_bucket_ptr->id == uint8_t(-1))
                    operator++();
            }
            Iter(const Iter&)               = default;
            ~Iter()                         = default;
            Iter& operator=(const Iter&)    = default;

            inline friend constexpr bool operator==(const Iter& lhs, const Iter& rhs) { return lhs.m_bucket_ptr == rhs.m_bucket_ptr; }
            inline friend constexpr bool operator!=(const Iter& lhs, const Iter& rhs) { return !(lhs == rhs); }
            inline Iter& operator++() 
            {
                while ((++m_bucket_ptr)->id == uint8_t(-1));
                return *this; 
            }
            inline const pointer operator->() const { return &(m_bucket_ptr->key_value_pair); }
            inline const_reference operator*() const { return m_bucket_ptr->key_value_pair; }
        private:
            friend class unordered_map;
            bucket_type* m_bucket_ptr;
        };

        friend class Iter;
        using iterator              = Iter;

        unordered_map()
            :m_allocator(std::nullopt),
            LOAD_FACTOR(0.8),
            GROWTH_FACTOR(2),
            m_capacity(64),
            m_max_elems(size_type(m_capacity * LOAD_FACTOR)),
            m_size(0)
        {
            initilizeBucket();
        }

        unordered_map(allocator_type& allocator, size_type inital_capacity = 64, float load_factor = 0.8, size_type growth_factor = 2)
            :m_allocator(std::ref(allocator)),
            LOAD_FACTOR(load_factor),
            GROWTH_FACTOR(closestPowerOfTwo(growth_factor)),
            m_capacity(closestPowerOfTwo(inital_capacity)),
            m_max_elems(size_type(m_capacity * LOAD_FACTOR)),
            m_size(0)
        { 
            initilizeBucket(); 
        }

        ~unordered_map()
        {
            for (bucket_type* iter = m_bucket; iter != end(); ++iter)
                if (iter->id != uint8_t(-1))
                    iter->key_value_pair.~value_type();

            m_allocator->get().deallocate(m_bucket, m_capacity + 1);
        }

        inline mapped_type& operator[](const key_type& key)
        {
            return const_cast<mapped_type&>(insert(value_type(key, mapped_type())).first->key_value_pair);
        }

        iterator find(const key_type& key) const
        {
            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key);
            while (true)
            {
                // Key not found
                if (JVN_UNLIKELY(iter->id == uint8_t(-1) || iter->id < id))
                    return end();

                // Key found
                if (iter->id == id && m_key_equal(iter->key_value_pair.first, key))
                    return iterator(iter);

                ++id;
                advanceIter(iter);
            }
        }

        // TODO: Add emplace support, rewrite insert based off emplace

        template <class Ty = value_type, typename = std::enable_if_t<std::is_convertible_v<Ty, value_type>>>
        std::pair<iterator, bool> insert(Ty&& key_value_pair)
        {
            bucket_type* return_iter = nullptr;

            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key_value_pair.first);
            while (true)
            {
                // Found an empty slot
                if (iter->id == uint8_t(-1))
                {
                    iter->id = id;
                    ::new (&(iter->key_value_pair)) auto(std::forward<value_type>(key_value_pair));

                    if (JVN_UNLIKELY(++m_size == m_max_elems))
                    {
                        // Record the original key before growth
                        if (return_iter)
                            key_value_pair.first = return_iter->key_value_pair.first;

                        grow();
                        return std::pair<iterator, bool>(find(key_value_pair.first), true);
                    }

                    return_iter = return_iter ? return_iter : iter;
                    return std::pair<iterator, bool>(iterator(return_iter), true);
                }

                // Key found
                if (iter->id == id && JVN_UNLIKELY(m_key_equal(key_value_pair.first, iter->key_value_pair.first)))
                    return std::pair<iterator, bool>(iterator(iter), false);

                // Swap rich with the poor
                if (iter->id < id)
                {
                    using std::swap;
                    swap(iter->id, id);
                    swap(iter->key_value_pair, key_value_pair);
                    return_iter = return_iter ? return_iter : iter;
                } 

                ++id;
                advanceIter(iter);
            }
        }

        size_type erase(const key_type& key)
        {
            iterator find_iter = find(key);

            // Check if elem is in the table
            if (find_iter == end())
                return size_type(0);

            // Traverse the bucket and swap elements with previous until
            // an empty slot is found or an element with the 0 hash distance
            bucket_type* iter = find_iter.m_bucket_ptr,
                    *prev_iter = find_iter.m_bucket_ptr;
            advanceIter(iter);
            while (iter->id != uint8_t(0) && iter->id != uint8_t(-1))
            {
                using std::swap;
                swap(*prev_iter, *iter);
                --(prev_iter->id);

                prev_iter = iter;
                advanceIter(iter);
            }

            // Erase the element
            prev_iter->id = uint8_t(-1);
            prev_iter->key_value_pair.~value_type();
            --m_size;

            return size_type(1);
        }

        inline size_type size() const noexcept { return m_size; }
        inline bool empty() const noexcept { return !m_size; }


        inline iterator begin() const { return iterator(m_bucket); }
        inline iterator end() const { return iterator(m_bucket_end); }

    private:
        const float LOAD_FACTOR;
        const size_type GROWTH_FACTOR;

        std::optional<std::reference_wrapper<allocator_type>> m_allocator;
        hasher m_hasher;
        key_equal m_key_equal;

        bucket_type* m_bucket;
        bucket_type* m_bucket_end;
        size_type m_capacity, m_size;
        // The number of elements that triggers grow()
        size_type m_max_elems;

        // Since m_capacity is always a power of two it's decremented value is all ones (1111....) binary.
        // It can be used for fast trimming of the top bits of the hash. 
        // % is a very slow operation so this is a very desired optimisation.
        // Care : The hash fucntion needs to not be reliant on the top bits otherwise colisions number will increase.
        inline size_type hashAndTrim(const key_type& key) const noexcept { return m_hasher(key) & (m_capacity - 1); }

        // Grows the map and rehashes it
        void grow()
        {
            size_type prev_capacity = m_capacity;
            bucket_type* prev_bucket = m_bucket,
                    *prev_bucket_end = m_bucket_end;

            // Grow the bucket
            m_capacity *= GROWTH_FACTOR;
            m_max_elems = m_capacity * LOAD_FACTOR;
            m_size = 0;
            initilizeBucket();

            // Rehash and insert
            for (auto iter = prev_bucket; iter != prev_bucket_end; ++iter)
                if (iter->id != uint8_t(-1))
                    insert(std::move(iter->key_value_pair));

            m_allocator->get().deallocate(prev_bucket, prev_capacity + 1);
        }

        void initilizeBucket()
        {
            // +1 is for the differantiation of the end() iterator
            m_bucket = m_allocator->get().allocate(m_capacity + 1);
            std::memset(m_bucket, uint8_t(-1), m_capacity * sizeof(*m_bucket));
                
            //Element at the end must have a non -1u info value
            m_bucket[m_capacity].id = uint8_t(0);

            m_bucket_end = m_bucket + m_capacity;
        }

        // Returns the first equal or bigger power of two. The return value is always greater than 1.
        static size_type closestPowerOfTwo(size_type num) noexcept
        {
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

        inline void advanceIter(bucket_type*& iter) const
        {
            ++iter;
            if (JVN_UNLIKELY(iter == m_bucket_end))
                    iter = m_bucket;
        }
    };

} // namespace jvn

#endif