#ifndef JVN_ROBIN_HOOD_MAP_
#define JVN_ROBIN_HOOD_MAP_

#include "hash.h"
#include <utility>
#include <cstring>
#include <memory>

namespace jvn
{
    template <class Kt, class Vt, 
            class Hasher = hash<Kt>, 
            class KeyEq = std::equal_to<Kt>, 
            class Alloc = std::allocator<std::pair<uint8_t, std::pair<Kt, Vt>>>>
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
        using bucket_type           = std::pair<uint8_t, value_type>;
        using allocator_type        = Alloc;
        using size_type             = typename Alloc::size_type;
        using difference_type       = typename Alloc::difference_type;
    public:

        class Iter
        {
        public:
            Iter(bucket_type* bucket_ptr): m_bucket_ptr(bucket_ptr)
            {   
                if (m_bucket_ptr->first == uint8_t(-1))
                    operator++();
            }
            Iter(const Iter&)               = default;
            ~Iter()                         = default;
            Iter& operator=(const Iter&)    = default;

            inline friend constexpr bool operator==(const Iter& lhs, const Iter& rhs) { return lhs.m_bucket_ptr == rhs.m_bucket_ptr; }
            inline friend constexpr bool operator!=(const Iter& lhs, const Iter& rhs) { return !(lhs == rhs); }
            inline Iter& operator++() 
            {
                while ((++m_bucket_ptr)->first == uint8_t(-1));
                return *this; 
            }
            inline const pointer operator->() const { return &(m_bucket_ptr->second); }
            inline const_reference operator*() const { return m_bucket_ptr->second; }
        private:
            friend class unordered_map;
            bucket_type* m_bucket_ptr;
        };

        friend class Iter;
        using iterator              = Iter;

        unordered_map(size_type inital_capacity, float load_factor, size_type growth_factor, allocator_type& allocator)
            :LOAD_FACTOR(load_factor),
            GROWTH_FACTOR(closestPowerOfTwo(growth_factor)),
            m_capacity(closestPowerOfTwo(inital_capacity)),
            m_allocator(allocator),
            m_max_elems(size_type(m_capacity * LOAD_FACTOR)),
            m_size(0)
        { 
            initilizeBucket(); 
        }

        ~unordered_map()
        {
            for (auto iter = m_bucket; iter != end(); ++iter)
                if (iter->first != uint8_t(-1))
                    iter->second.~value_type();

            m_allocator.deallocate(m_bucket, m_capacity + 1);
        }

        inline mapped_type& operator[](const key_type& key)
        {
            return const_cast<mapped_type&>(insert(value_type(key, mapped_type())).first->second);
        }

        iterator find(const key_type& key) const
        {
            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key);
            while (true)
            {
                // Key not found
                if (JVN_UNLIKELY(iter->first == uint8_t(-1) || iter->first < id))
                    return end();

                // Key found
                if (iter->first == id && key_equal{}(iter->second.first, key))
                    return iterator(iter);

                ++id;
                advanceIter(iter);
            }
        }

        template <class Ty, std::enable_if_t<std::is_same<std::decay_t<Ty>, value_type>::value, int> = 0>
        std::pair<iterator, bool> insert(Ty&& key_value_pair)
        {
            bucket_type* return_iter = nullptr;

            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key_value_pair.first);
            while (true)
            {
                // Found an empty slot
                if (iter->first == uint8_t(-1))
                {
                    iter->first = id;
                    ::new (&(iter->second)) auto(std::forward<value_type>(key_value_pair));

                    if (JVN_UNLIKELY(++m_size == m_max_elems))
                    {
                        // Record the original key before growth
                        if (return_iter)
                            key_value_pair.first = return_iter->second.first;

                        grow();
                        return std::pair<iterator, bool>(find(key_value_pair.first), true);
                    }

                    return_iter = return_iter ? return_iter : iter;
                    return std::pair<iterator, bool>(iterator(return_iter), true);
                }

                // Key found
                if (iter->first == id && JVN_UNLIKELY(key_equal{}(key_value_pair.first, iter->second.first)))
                    return std::pair<iterator, bool>(iterator(iter), false);

                // Swap rich with the poor
                if (iter->first < id)
                {
                    using std::swap;
                    swap(iter->first, id);
                    swap(iter->second, key_value_pair);
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
            while (iter->first != uint8_t(0) && iter->first != uint8_t(-1))
            {
                using std::swap;
                swap(*prev_iter, *iter);
                --(prev_iter->first);

                prev_iter = iter;
                advanceIter(iter);
            }

            // Erase the element
            prev_iter->first = uint8_t(-1);
            prev_iter->second.~value_type();
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


        bucket_type* m_bucket;
        bucket_type* m_bucket_end;
        allocator_type& m_allocator;
        hasher m_hasher;
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
                if (iter->first != uint8_t(-1))
                    insert(std::move(iter->second));

            m_allocator.deallocate(prev_bucket, prev_capacity + 1);
        }

        void initilizeBucket()
        {
            // +1 is for the differantiation of the end() iterator
            m_bucket = m_allocator.allocate(m_capacity + 1);
            std::memset(m_bucket, uint8_t(-1), m_capacity * sizeof(*m_bucket));
                
            //Element at the end must have a non -1u info value
            m_bucket[m_capacity].first = uint8_t(0);

            m_bucket_end = m_bucket + m_capacity;
        }

        // Returns the first equal or bigger power of two. The return value is always greater than 1.
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

        inline void advanceIter(bucket_type*& iter) const
        {
            ++iter;
            if (JVN_UNLIKELY(iter == m_bucket_end))
                    iter = m_bucket;
        }
    };

} // namespace jvn

#endif