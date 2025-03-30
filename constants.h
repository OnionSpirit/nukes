#ifndef NUKES_CONSTANTS
#define NUKES_CONSTANTS



namespace nukes::constants {


inline constexpr size_t ufl_memory_offset = 16; ///< Memory offset for unbounded
                                                ///< freelist.
                                                ///< freelist top ptr size and
                                                ///< next bucket node ptr size

// NOTE: Размер машинного слова
inline constexpr size_t machine_word_size = sizeof(size_t);

// NOTE: CT функция поиска минимальной степени двойки от размера объекта
inline consteval size_t min_2_power (size_t len) { return std::bit_ceil(len); }

// NOTE: Рассчётное выравнивание по машинному слову 
template <typename T>
inline constexpr size_t word_alignment = sizeof(T) < machine_word_size
        ? machine_word_size
        : std::bit_ceil(sizeof(T));

// NOTE: Спец значение используемое для режима аллокации внутренних буферов в конструкторе
inline constexpr uint32_t runtime_discover = 0;

inline constexpr size_t bucket_meta_data = 8; ///< Handles bucket ptr

} // end namespace nukes::constants

#endif // NUKES_CONSTANTS
