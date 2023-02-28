#ifndef JVN_ROBIN_HOOD_UTILITY_
#define JVN_ROBIN_HOOD_UTILITY_

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

// pragma
#ifdef _MSC_VER
    #define JVN_PRAGMA_PACK_PUSH(n)  __pragma(pack(push, n))
    #define JVN_PRAGMA_PACK_POP()    __pragma(pack(pop))
#else
    #define JVN_PRAGMA_PACK_PUSH(n) _Pragma("pack(push, n)")
    #define JVN_PRAGMA_PACK_POP()   _Pragma("pack(pop)")
#endif

// End custom macro  -----------------------------------

#endif