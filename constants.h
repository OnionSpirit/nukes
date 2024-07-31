#ifndef NUKES_CONSTANTS
#define NUKES_CONSTANTS



namespace nukes::constants {


inline constexpr size_t ufl_memory_offset = 16; ///< Memory offset for unbounded
                                                ///< freelist.
                                                ///< freelist top ptr size and
                                                ///< next bucket node ptr size

} // end namespace nukes::constants

#endif // NUKES_CONSTANTS
