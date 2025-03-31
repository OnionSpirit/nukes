#ifndef NUKES_CONSTANTS
#define NUKES_CONSTANTS

#include <cstdint>
#include <bit>
#include <limits>

namespace nukes::details::constants {


inline constexpr std::size_t ufl_memory_offset = 16; ///< Memory offset for unbounded
    ///< freelist.
    ///< freelist top ptr size and
    ///< next bucket node ptr size

// NOTE: Размер машинного слова
inline constexpr std::size_t word_size = sizeof(std::size_t);

// NOTE: Размер половины машинного слова
inline constexpr std::size_t hword_size = sizeof(std::size_t);

// NOTE: Целочисленный тип соответствующий машинному слову
using word = decltype([]() consteval {
    if constexpr (word_size == 8) return uint64_t();
    else if constexpr (word_size == 4) return uint32_t();
    else if constexpr (word_size == 2) return uint16_t();
    else return uint64_t(); } () );

// NOTE: Целочисленный тип соответствующий половине машинного слова
using hword = decltype([]() consteval {
    if constexpr (word_size == 8) return uint32_t();
    else if constexpr (word_size == 4) return uint16_t();
    else if constexpr (word_size == 2) return uint8_t();
    else return uint32_t(); } () );

// NOTE: Минимальное значение в машинном слове
inline constexpr word word_min_v = 0;

// NOTE: Минимальное значение в половине машинного слова
inline constexpr hword hword_min_v = 0;

// NOTE: Максимальное значение в машинном слове
inline constexpr word word_max_v = std::numeric_limits<word>::max();

// NOTE: Максимальное значение в половине машинного слова
inline constexpr hword hword_max_v = std::numeric_limits<hword>::max();

// NOTE: CT функция поиска минимальной степени двойки от размера объекта
inline consteval std::size_t min_2_power (std::size_t len) { return std::bit_ceil(len); }

// NOTE: Рассчётное выравнивание по машинному слову 
template <typename T>
inline constexpr std::size_t word_alignment = sizeof(T) < word_size
        ? word_size
        : std::bit_ceil(sizeof(T));

// NOTE: Спец значение используемое для режима аллокации внутренних буферов в конструкторе
inline constexpr uint32_t runtime_discover = 0;

inline constexpr std::size_t bucket_meta_data = 8; ///< Handles bucket ptr

} // end namespace nukes::constants

#endif // NUKES_CONSTANTS
