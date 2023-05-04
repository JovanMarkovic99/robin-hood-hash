#ifndef JVN_ROBIN_HOOD_MAP_
#define JVN_ROBIN_HOOD_MAP_

#include "hash.h"
#include <utility>
#include <cstring>

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

        unordered_map() { 
            initilize(16); 
        }

        unordered_map(size_type inital_capacity, float load_factor = 0.8f, size_type growth_factor = 2, allocator_type allocator = allocator_type())
            :M_LOAD_FACTOR(load_factor),
            M_GROWTH_FACTOR(closestPowerOfTwo(growth_factor)),
            m_allocator(allocator) { 
            initilize(closestPowerOfTwo(inital_capacity)); 
        }

        unordered_map(const unordered_map& m)
            :M_LOAD_FACTOR(m.M_LOAD_FACTOR),
            M_GROWTH_FACTOR(m.M_GROWTH_FACTOR),
            m_allocator(m_allocator) {
            m_bucket = m_allocator.allocate(m.m_dec_capacity + 2);
            if (m_bucket == nullptr)
                throw std::bad_alloc();

            m_dec_capacity = m.m_dec_capacity;
            m_max_elems = m.m_max_elems;
            m_size = m.m_size;
            m_bucket_end = m_bucket + m_dec_capacity + 2;

            std::memcpy(m_bucket, m.m_bucket, m_bucket_end - m_bucket);
            m_bucket_end->id = uint8_t(0);
        }

        unordered_map(unordered_map&& m)
            :M_LOAD_FACTOR(m.M_LOAD_FACTOR),
            M_GROWTH_FACTOR(m.M_GROWTH_FACTOR),
            m_allocator(std::move(m.m_allocator)),
            m_bucket(m.m_bucket),
            m_bucket_end(m.m_bucket_end),
            m_dec_capacity(m.m_dec_capacity),
            m_size(m.m_size),
            m_max_elems(m.m_max_elems){
            m.m_bucket = nullptr;
            m.m_bucket_end = nullptr;
        }

        ~unordered_map() {
            for (bucket_type* iter = m_bucket; iter != m_bucket_end; ++iter)
                if (iter->id != uint8_t(-1))
                    iter->key_value_pair.~m_value_type();

            m_allocator.deallocate(m_bucket, m_dec_capacity + 2);
        }

        void reserve(size_type size) {
            size = loadedCapacity(size);
            if (size > m_dec_capacity)
                growTo(closestPowerOfTwo(size));
        }

        template <class KeyTy>
        inline mapped_type& operator[](KeyTy&& key) {
            return insert(m_value_type(std::forward<KeyTy>(key), mapped_type())).first->second;
        }

        template <class... Valtys>
        inline std::pair<iterator, bool> emplace(Valtys&&... vals) {
            return insert(m_value_type(std::forward<Valtys>(vals)...));
        }

        template <class ValTy = m_value_type>
        std::pair<iterator, bool> insert(ValTy&& key_value_pair) {
            if (JVN_UNLIKELY(m_size == m_max_elems))
                growTo((m_dec_capacity + 1) * M_GROWTH_FACTOR);

            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key_value_pair.first);
            while (true) {
                // Empty slot found
                if (iter->id == uint8_t(-1)) {
                    iter->id = id;
                    ::new (&(iter->key_value_pair)) m_value_type(std::forward<ValTy>(key_value_pair));
                    break;
                }

                // Rich found
                if (iter->id < id) {
                    insertFrom(advanceIter(iter), iter->id + 1, std::move(iter->key_value_pair));
                    iter->id = id;
                    iter->key_value_pair = std::forward<ValTy>(key_value_pair);
                    break;
                } 

                // Key found
                if (JVN_UNLIKELY(iter->id == id && m_key_equal(key_value_pair.first, iter->key_value_pair.first)))
                    return std::pair<iterator, bool>(iterator(iter), false);

                ++id;
                iter = advanceIter(iter);
            }
    
            ++m_size;
            return std::pair<iterator, bool>(iterator(iter), true);
        }


        template <class KeyTy>
        size_type erase(KeyTy&& key) noexcept {
            bucket_type* iter = find(std::forward<KeyTy>(key)).m_bucket_ptr;
            if (iter == m_bucket_end)
                return size_type(0);

            // Traverse the bucket and swap elements with previous until
            // an empty slot is found or an element with the 0 hash distance
            bucket_type* next_iter = advanceIter(iter);
            while (next_iter->id != uint8_t(0) && next_iter->id != uint8_t(-1)) {
                iter->id = next_iter->id - 1;
                using std::swap;
                swap(iter->key_value_pair, next_iter->key_value_pair);

                iter = std::exchange(next_iter, advanceIter(next_iter));
            }

            iter->id = uint8_t(-1);
            iter->key_value_pair.~m_value_type();
            --m_size;

            return size_type(1);
        }

        template <class KeyTy>
        iterator find(KeyTy&& key) const noexcept {
            uint8_t id = 0;
            bucket_type* iter = m_bucket + hashAndTrim(key);
            while (true) {
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
        float M_LOAD_FACTOR         = 0.8f;
        size_type M_GROWTH_FACTOR   = 2;

        allocator_type m_allocator;
        hasher m_hasher;
        key_equal m_key_equal;

        bucket_type* m_bucket       = nullptr;
        bucket_type* m_bucket_end   = nullptr;

        size_type m_dec_capacity    = 0;
        size_type m_size            = 0;
        // The number of elements that triggers growth
        size_type m_max_elems       = 0;

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
            ::new (&(iter->key_value_pair)) m_value_type(std::move(key_value_pair));
        }

        // Since m_dec_capacity is always a power of two - 1, it's  value is all ones binary
        // It can be used for fast trimming of the top bits of the hash, since % is a slow operation
        template <class KeyTy>
        inline size_type hashAndTrim(KeyTy&& key) const noexcept { return m_hasher(std::forward<KeyTy>(key)) & m_dec_capacity; }

        inline bucket_type* advanceIter(bucket_type* iter) const noexcept {
            if (JVN_UNLIKELY(++iter == m_bucket_end))
                    return m_bucket;

            return iter;
        }

        void growTo(size_t new_capacity) {
            bucket_type* prev_bucket = m_bucket,
                    *prev_bucket_end = m_bucket_end;

            initilize(new_capacity);

            // Re-insert
            for (auto iter = prev_bucket; iter != prev_bucket_end; ++iter)
                if (iter->id != uint8_t(-1))
                    insert(std::move(iter->key_value_pair));

            m_allocator.deallocate(prev_bucket, size_type(prev_bucket_end - prev_bucket + 1));
        }

        // Initilizes the map with a certain size. Map is unchanged on bad_alloc()
        void initilize(size_type capacity) {
            // +1 is for the differantiation of the end() iterator
            bucket_type* new_bucket = m_allocator.allocate(capacity + 1);
            if (new_bucket == nullptr)
                throw new std::bad_alloc();

            m_dec_capacity = capacity - 1;
            m_max_elems = size_type(float(capacity) * M_LOAD_FACTOR);
            m_size = 0;

            m_bucket = new_bucket;
            m_bucket_end = m_bucket + capacity;
            for (bucket_type* iter = m_bucket; iter != m_bucket_end; ++iter)
                iter->id = uint8_t(-1);
                
            //Element at the end must have a non -1u info value
            m_bucket_end->id = uint8_t(0);
        }

        // Returns the wanted capacity factoring for M_LOAD_FACTOR
        inline size_type loadedCapacity(size_type capacity) noexcept {
            return size_type(std::ceil(float(capacity) / M_LOAD_FACTOR));
        }

        // Returns the first equal or bigger power of two. The return value is always greater than 1
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