#ifndef JVN_ROBIN_HOOD_MAP_
#define JVN_ROBIN_HOOD_MAP_

#include "hash.h"
#include <utility>
#include <memory>

namespace jvn
{
    template <class Kt, class Vt, class Hasher = hash<Kt>,
        class KeyEq = std::equal_to<Kt>, class Alloc = std::allocator<std::pair<Kt, Vt>>>
        class unordered_map
    {
    public:
        using hasher                = Hasher;
        using mapped_type           = Vt;
        using key_type              = Kt;
        using key_equal             = KeyEq;
        using value_type            = std::pair<Kt, Vt>;
        using pointer               = value_type*;
        using reference             = value_type&;
        using const_reference       = const value_type&;
        using size_type             = typename Alloc::size_type;
        using difference_type       = typename Alloc::difference_type;
        using allocator_type        = Alloc;
    private:
        using byte_allocator_type   = typename Alloc::template rebind<uint8_t>::other;
    public:

        class Iter
        {
        public:
            Iter(uint8_t* info_ptr, value_type* value_ptr): m_info_ptr(info_ptr), m_value_ptr(value_ptr) 
            { 
                if (*m_info_ptr == uint8_t(-1))
                    operator++();
            }
            Iter(const Iter&)               = default;
            ~Iter()                         = default;
            Iter& operator=(const Iter&)    = default;

            friend constexpr bool operator==(const Iter& lhs, const Iter& rhs) { return lhs.m_info_ptr == rhs.m_info_ptr; }
            friend constexpr bool operator!=(const Iter& lhs, const Iter& rhs) { return !(lhs == rhs); }
            Iter& operator++() 
            {
                advance_iter(m_info_ptr), advance_iter(m_value_ptr);
                while (*m_info_ptr == uint8_t(-1))
                    advance_iter(m_info_ptr), advance_iter(m_value_ptr);
                return *this; 
            }
            const pointer operator->() const { return &*m_value_ptr; }
            const_reference operator*() const { return *m_value_ptr; }
        private:
            friend class unordered_map;
            value_type* m_value_ptr;
            uint8_t* m_info_ptr;
        };

        friend class Iter;
        using iterator              = Iter;

        unordered_map(float load_factor = 0.75f, size_type inital_capacity = 128u, size_type growth_factor = 16u)
            :LOAD_FACTOR(load_factor),
            INITIAL_CAPACITY(closestPowerOfTwo(inital_capacity)),
            GROWTH_FACTOR(closestPowerOfTwo(growth_factor)),
            m_capacity(INITIAL_CAPACITY),
            m_capacity_dec(INITIAL_CAPACITY - 1),
            m_max_elems(size_type(INITIAL_CAPACITY* LOAD_FACTOR)),
            m_size(0)
        {
            // +1 is for the differantiation of the end() iterator
            m_info_bucket = m_allocator.allocate(m_capacity * ELEM_OFFSET + 1);

            m_bucket = reinterpret_cast<value_type*>(m_info_bucket + 1);
            auto iter = m_info_bucket;
            for (; iter != end(m_info_bucket); advance_iter(iter))
                *iter = uint8_t(-1);
                
            //Element at the end must have a non -1u info value
            *iter = uint8_t(0);
        }

        ~unordered_map()
        {
            auto iter_info = m_info_bucket;
            auto iter_bucket = m_bucket;
            for (; iter_info != end(m_info_bucket); advance_iter(iter_info, iter_bucket))
                    if (*iter_info != uint8_t(-1))
                        iter_bucket->~value_type();
            m_allocator.deallocate(m_info_bucket, m_capacity * ELEM_OFFSET + 1);
        }

        mapped_type& operator[](const key_type& key)
        {
            return const_cast<mapped_type&>(insert(value_type(key, mapped_type())).first->second);
        }

        iterator find(const key_type& key) const
        {
            auto idx = hashAndTrim(key);

            // Distance from hash position
            auto id = uint8_t(0);

            auto iter_info = m_info_bucket;
            auto iter_bucket = m_bucket;
            advance_iter(iter_info, idx);
            advance_iter(iter_bucket, idx);
            while (true)
            {
                // Key not found
                if (JVN_UNLIKELY(*iter_info == uint8_t(-1) || *iter_info < id))
                    return end();

                // Key found
                if (*iter_info == id && key_equal{}(iter_bucket->first, key))
                    return iterator(iter_info, iter_bucket);

                ++id;
                advance_iter(iter_info, iter_bucket);

                // Check iterator bounds
                if (JVN_UNLIKELY(iter_info == end(m_info_bucket)))
                    iter_info = m_info_bucket, iter_bucket = m_bucket;
            }
        }

        template <class Ty, std::enable_if_t<std::is_same<std::decay_t<Ty>, value_type>::value, int> = 0>
        std::pair<iterator, bool> insert(Ty&& key_value_pair)
        {
            auto idx = hashAndTrim(key_value_pair.first);

            // Distance from hash position
            auto id = uint8_t(0);

            // Original key
            auto key = key_value_pair.first;

            auto iter_info = m_info_bucket;
            auto iter_bucket = m_bucket;
            advance_iter(iter_info, idx);
            advance_iter(iter_bucket, idx);
            while (true)
            {
                // Found an empty slot
                if (*iter_info == uint8_t(-1))
                {
                    *iter_info = id;
                    ::new (iter_bucket) auto(std::forward<Ty>(key_value_pair));
                    ++m_size;
                    if (JVN_UNLIKELY(m_size == m_max_elems))
                        grow();
                    return std::pair<iterator, bool>(find(key), true);
                }

                // Key found
                if (*iter_info == id && JVN_UNLIKELY(key_equal{}(iter_bucket->first, key_value_pair.first)))
                    return std::pair<iterator, bool>(iterator(iter_info, iter_bucket), false);

                // Swap rich with the poor
                if (*iter_info < id)
                {
                    swap(*iter_bucket, key_value_pair);
                    std::swap(*iter_info, id);
                } 

                ++id;
                advance_iter(iter_info, iter_bucket);

                // Check iterator bounds
                if (JVN_UNLIKELY(iter_info == end(m_info_bucket)))
                    iter_info = m_info_bucket, iter_bucket = m_bucket;
            }
        }

        size_type erase(const key_type& key)
        {
            auto iter = find(key);

            // Check if elem is in the table
            if (iter == end())
                return size_type(0);

            // Traverse the bucket and swap elements with previous until
            // an empty slot is found or an element with the 0 hash distance
            auto iter_info = iter.m_info_ptr;
            auto iter_bucket = iter.m_value_ptr;
            auto iter_prev_info = iter_info;
            auto iter_prev_bucket = iter_bucket;
            advance_iter(iter_info, iter_bucket);
            if (JVN_UNLIKELY(iter_info == end(m_info_bucket)))
                    iter_info = m_info_bucket, iter_bucket = m_bucket;
            while (*iter_info != uint8_t(0) && *iter_info != uint8_t(-1))
            {
                std::swap(*iter_prev_info, *iter_info);
                swap(*iter_prev_bucket, *iter_bucket);
                --(*iter_prev_info);
                advance_iter(iter_prev_info, iter_prev_bucket, iter_info, iter_bucket);
                if (JVN_UNLIKELY(iter_prev_info == end(m_info_bucket)))
                    iter_prev_info = m_info_bucket, iter_prev_bucket = m_bucket;
                if (JVN_UNLIKELY(iter_info == end(m_info_bucket)))
                    iter_info = m_info_bucket, iter_bucket = m_bucket;
            }

            // Destroy the element
            *iter_prev_info = uint8_t(-1);
            iter_prev_bucket->~value_type();
            --m_size;
            return size_type(1);
        }

        size_type size() const noexcept { return m_size; }
        bool empty() const noexcept { return !m_size; }


        iterator begin() const { return iterator(m_info_bucket, m_bucket); }
        iterator end() const { return iterator(end(m_info_bucket), nullptr); }

    private:
        // Number of bytes needed to advance a pointer for the next element
        static JVN_INLINE_VAR constexpr size_type ELEM_OFFSET = 1 + sizeof(value_type);

        const float LOAD_FACTOR;

        // Must be power of 2 for optimal hash trimming
        const size_type INITIAL_CAPACITY;
        const size_type GROWTH_FACTOR;

        // A decremented value of m_capacity used for hash trimming
        size_type m_capacity_dec;
        // The number of elements that triggers grow()
        size_type m_max_elems;

        size_type m_capacity, m_size;
        value_type* m_bucket;
        uint8_t* m_info_bucket;
        byte_allocator_type m_allocator;

        // Since m_capacity is always a power of two and m_capacity_dec is just its decremented value
        // m_capacity_dec is all ones (1111....) binary, it can be used for fast trimming of the top bits
        // of the hash. % is a very slow operation so this is a very desired optimisation
        // Care : The hash fucntion needs to not be reliant on the top bits otherwise colisions number will
        // increase
        size_type hashAndTrim(const key_type& key) const noexcept { return hasher{}(key) & m_capacity_dec; }

        // Grows the map and rehashes it
        void grow()
        {
            auto prev_capacity = m_capacity;
            auto prev_bucket = m_bucket;
            auto prev_info_bucket = m_info_bucket;

            // Grow the bucket
            m_capacity = size_type(prev_capacity * GROWTH_FACTOR);
            m_capacity_dec = m_capacity - 1;
            m_max_elems = size_type(m_capacity * LOAD_FACTOR);
            m_size = 0;
            m_info_bucket = m_allocator.allocate(m_capacity * ELEM_OFFSET + 1);
            m_bucket = reinterpret_cast<value_type*>(m_info_bucket + 1);
            auto iter = m_info_bucket;
            for (; iter != end(m_info_bucket); advance_iter(iter))
                *iter = uint8_t(-1);
            *iter = uint8_t(0);

            // Rehash and insert
            auto iter_info = prev_info_bucket;
            auto iter_bucket = prev_bucket;
            auto end_iter = prev_info_bucket;
            advance_iter(end_iter, prev_capacity);
            for (; iter_info != end_iter; advance_iter(iter_info, iter_bucket))
                if (*iter_info != uint8_t(-1))
                    insert(std::move(*iter_bucket));
            m_allocator.deallocate(prev_info_bucket, prev_capacity * ELEM_OFFSET + 1);
        }

        // Returns the first equal or bigger power of two, return value is always greater than 1
        static size_type closestPowerOfTwo(size_type num) noexcept
        {
            if (num < 2)
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

        // When the pointer to the beggining of the bucket is passed it returns the pointer to the end
        template <class Ty, std::enable_if_t<std::is_pointer<Ty>::value, int> = 0>
        Ty constexpr end(Ty ptr) const noexcept { return advance_iter(ptr, m_capacity); }

        // Advance pointer(s) by count elements
        template <class Ty, std::enable_if_t<std::is_pointer<Ty>::value, int> = 0>
        static constexpr Ty advance_iter(Ty& ptr, size_type count = 1) noexcept 
        { return ptr = reinterpret_cast<Ty>(reinterpret_cast<int8_t*>(ptr) + count * ELEM_OFFSET); }
        template <class ...Tn>
        static constexpr void advance_iter(Tn&... rest) noexcept
        { ignore(advance_iter(rest)...); }
        template <class ...Tn>
        static constexpr void ignore(Tn&&... rest) {}
    };

} // namespace jvn

#endif