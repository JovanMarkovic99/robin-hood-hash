#ifndef JVN_ROBIN_HOOD_MEMORY_
#define JVN_ROBIN_HOOD_MEMORY_

#include <cstdlib>

namespace jvn
{

    // A fixed block memeory allocator that allocates off of alternating sides of the block
    template<typename Ty>
    class AlternatingFixedMemoryAllocator {
    public:
        using value_type = Ty;
        using pointer = value_type*;
        using const_pointer = const value_type*;
        using reference = value_type&;
        using const_reference = const value_type&;
        using difference_type = std::ptrdiff_t;
        using size_type = std::size_t;

        const size_type Capacity;

        AlternatingFixedMemoryAllocator(size_type capacity)
            :Capacity(capacity),
            m_alloc_left(true)                              
        {
            m_data = static_cast<pointer>(_aligned_malloc(Capacity * sizeof(*m_data), alignof(*m_data)));
            if (!m_data)
                throw std::bad_alloc();
            m_left = m_data;
            m_right = m_data + Capacity;
        }

        ~AlternatingFixedMemoryAllocator() {
            _aligned_free(static_cast<void*>(m_data));
        }
        
        AlternatingFixedMemoryAllocator(const AlternatingFixedMemoryAllocator&) = delete;
        AlternatingFixedMemoryAllocator operator=(const AlternatingFixedMemoryAllocator&) = delete;
        
        pointer allocate(size_type n) {
            if (m_left + n > m_right)
                return nullptr;

            if (m_alloc_left) {
                m_left += n;
                m_alloc_left = false;
                return m_left - n;
            }
            
            m_right -= n;
            m_alloc_left = true;
            return m_right;
        }
        
        void deallocate(pointer ptr, size_type n) {   
            if (ptr == m_right) {
                m_right += n;
                m_alloc_left = false;
            }
            else if (ptr + n == m_left) {
                m_left -= n;
                m_alloc_left = true;
            }
        }
        
    private:
        pointer m_data,
                m_left,
                m_right;
        bool    m_alloc_left;
    };


} // namespace jvn


#endif