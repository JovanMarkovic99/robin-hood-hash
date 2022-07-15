#ifndef JVN_ROBIN_HOOD_HASH_
#define JVN_ROBIN_HOOD_HASH_

#include <string>
#include <stdint.h>

// Define custom macro  --------------------------------

#define JVN(x) JVN_DEFINITION_##x()

// inline variables
// Check if C++17 or greater
#if (__cplusplus >= 201703L) || (defined(_MSVC_LANG) && (_MSVC_LANG >= 201703L))
#   define JVN_INLINE_VAR inline
#else
#   define JVN_INLINE_VAR
#endif

// bitness
#if SIZE_MAX == UINT64_MAX
#    define JVN_DEFINITION_BITNESS() 64
#elif SIZE_MAX == UINT32_MAX
#    define JVN_DEFINITION_BITNESS() 32
#else
#    error Unsupported bitness
#endif

// likely/unlikely
#ifdef _MSC_VER
#    define JVN_LIKELY(condition) condition
#    define JVN_UNLIKELY(condition) condition
#else
#    define JVN_LIKELY(condition) __builtin_expect(condition, 1)
#    define JVN_UNLIKELY(condition) __builtin_expect(condition, 0)
#endif

// End custom macro  -----------------------------------

namespace jvn
{

// FNV-1a hash algorithm
namespace fnv
{

// FNV-1a utility
#if JVN(BITNESS) == 64
    JVN_INLINE_VAR constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
    JVN_INLINE_VAR constexpr size_t FNV_PRIME        = 1099511628211ULL;
#else
    JVN_INLINE_VAR constexpr size_t FNV_OFFSET_BASIS = 2166136261U;
    JVN_INLINE_VAR constexpr size_t FNV_PRIME        = 16777619U;
#endif
    
inline size_t fnv_1a(size_t val, const unsigned char* bytes, size_t count) noexcept
{
    for (size_t i = 0; i < count; ++i)
    {
        val ^= bytes[i];
        val *= FNV_PRIME;
    }
    return val;
}
} // namespace fnv

// MurmurHash hash algorithm
namespace murmur_hash
{

// Seed for MurmurHash64A and MurmurHashNeutral2
JVN_INLINE_VAR constexpr size_t SEED = 0xe17a1465;

#if JVN(BITNESS) == 64

    //  TODO : Check if endian dependent
    //  MurmurHash64A
    JVN_INLINE_VAR constexpr size_t m = 0xc6a4a7935bd1e995;
    JVN_INLINE_VAR constexpr size_t r = 47;
    size_t murmur_hash2(const unsigned char* bytes, size_t count) noexcept
    {
        size_t hash = SEED ^ (count * m);

        const size_t* data = reinterpret_cast<const size_t*>(bytes);
        const size_t* end = data + (count / 8);

        while(data != end)
        {
            size_t k = *data++;

            k *= m; 
            k ^= k >> r; 
            k *= m; 
            
            hash ^= k;
            hash *= m; 
        }

        const unsigned char* data2 = reinterpret_cast<const unsigned char*>(data);

        switch(count & 7)
        {
        case 7: hash ^= size_t(data2[6]) << 48;
        case 6: hash ^= size_t(data2[5]) << 40;
        case 5: hash ^= size_t(data2[4]) << 32;
        case 4: hash ^= size_t(data2[3]) << 24;
        case 3: hash ^= size_t(data2[2]) << 16;
        case 2: hash ^= size_t(data2[1]) << 8;
        case 1: hash ^= size_t(data2[0]);
                hash *= m;
        };
    
        hash ^= hash >> r;
        hash *= m;
        hash ^= hash >> r;

        return hash;
    } 

    // MurmurHash3 Int64 mix
    inline size_t murmur_hash3_int(size_t k) noexcept
    {
        k ^= k >> 33;
        k *= 0xff51afd7ed558ccd;
        k ^= k >> 33;
        k *= 0xc4ceb9fe1a85ec53;
        k ^= k >> 33;
        return k;
    }

#else

    // MurmurHashNeutral2
    JVN_INLINE_VAR constexpr size_t m = 0x5bd1e995;
    JVN_INLINE_VAR constexpr size_t r = 24;
    size_t murmur_hash2(const unsigned char* bytes, size_t count) noexcept
    {
        size_t hash = SEED ^ count;

        while(count >= 4)
        {
            size_t k;

            k  = bytes[0];
            k |= bytes[1] << 8;
            k |= bytes[2] << 16;
            k |= bytes[3] << 24;

            k *= m; 
            k ^= k >> r; 
            k *= m;

            hash *= m;
            hash ^= k;

            bytes += 4;
            count -= 4;
        }
        
        switch(count)
        {
        case 3: hash ^= bytes[2] << 16;
        case 2: hash ^= bytes[1] << 8;
        case 1: hash ^= bytes[0];
                hash *= m;
        };

        hash ^= hash >> 13;
        hash *= m;
        hash ^= hash >> 15;

        return hash;
    } 

    // MurmurHash3 Int32 mix
    inline size_t murmur_hash3_int(size_t h) noexcept
    {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

#endif

} // namespace murmur_hash


// A function object that returns the hash of Kt type
// By default it uses the MurmurHash2/3 algorithm
// There is also THE FNV-1a implementation fnv::fnv_1a(...)
template <class Kt>
struct hash
{
    // General byte hashing
    size_t operator()(const Kt& key) const noexcept
    {
        return murmur_hash::murmur_hash2(&reinterpret_cast<const unsigned char&>(key), sizeof(key));
    }
};


// Type specialization

// There's probably a better way with SFINAE or some other conditional
// enabling. But this is a pretty easy way to do specific specialization
#define JVN_HASH_INT(Ty)                                        \
    template <>                                                 \
    struct hash<Ty>                                             \
    {                                                           \
        size_t operator()(const Ty key) const noexcept          \
        {                                                       \
            return murmur_hash::murmur_hash3_int(size_t(key));  \
        }                                                       \
    }                                                           

JVN_HASH_INT(bool);
JVN_HASH_INT(char);
JVN_HASH_INT(signed char);
JVN_HASH_INT(unsigned char);
JVN_HASH_INT(char16_t);
JVN_HASH_INT(char32_t);
JVN_HASH_INT(wchar_t);
JVN_HASH_INT(short);
JVN_HASH_INT(unsigned short);
JVN_HASH_INT(int);
JVN_HASH_INT(unsigned int);
JVN_HASH_INT(long);
JVN_HASH_INT(long long);
JVN_HASH_INT(unsigned long);
JVN_HASH_INT(unsigned long long);

template <class Ty>
struct hash<Ty*> {
    size_t operator()(Ty* ptr) const noexcept
    {
        return murmur_hash::murmur_hash3_int(size_t(ptr));
    }
};

template <>
struct hash<std::string>
{
    size_t operator()(const std::string& str) const noexcept
    {
        return murmur_hash::murmur_hash2(
            reinterpret_cast<const unsigned char*>(str.data()), str.size());
    }
};

} // namespace jvn

#endif