#ifndef NUKES_CONSTANTS
#define NUKES_CONSTANTS



namespace nukes::constants {


inline constexpr size_t ufl_memory_offset = 16; ///< Memory offset for unbounded
                                                ///< freelist.
                                                ///< freelist top ptr size and
                                                ///< next bucket node ptr size

inline constexpr size_t machine_word_size = sizeof(size_t);

template <typename T>
inline constexpr size_t word_alignment = sizeof(T) < machine_word_size
        ? machine_word_size
        : std::bit_ceil(sizeof(T));


} // end namespace nukes::constants

#endif // NUKES_CONSTANTS
