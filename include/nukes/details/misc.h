#ifndef NUKES_MISC
#define NUKES_MISC

#include <atomic>
#include <bit>

#include "constants.h"



namespace nukes::details::misc {


    // NOTE: Mixin класс для алиасинга атомарного типа
    template<typename NextType>
    struct atomic_typedef_mixin { typedef std::atomic<NextType> atomic_t; };

    // NOTE: Вспомогательный тип для определения наиболее дешевой по памяти сигнатуры функций
    template<typename T>
    class argument {

        // NOTE: Проверка может ли объект типа копироваться или только переноситься
        static constexpr bool is_rvalue_type =
                not (std::is_copy_assignable_v<std::decay_t<T>> or std::is_copy_constructible_v<std::decay_t<T>>)
                and (std::is_move_assignable_v<std::decay_t<T>> or std::is_move_constructible_v<std::decay_t<T>>);

        // NOTE: Проверка является ли тип меньше чем размер машинного слова
        static constexpr bool is_short_type = sizeof(T) <= constants::word_size;

    public:

        typedef std::conditional_t<is_rvalue_type, T &&,
                std::conditional_t<is_short_type, T, T &>
        > type;
    };

    template<typename T>
    using argument_t = argument<T>::type;

    // NOTE: Вспомогательный тип для определения наиболее дешевой по памяти сигнатуры функций
    template<typename T>
    class argument_ref {

        // NOTE: Проверка может ли объект типа копироваться или только переноситься
        static constexpr bool is_rvalue_type =
                not (std::is_copy_assignable_v<std::decay_t<T>> or std::is_copy_constructible_v<std::decay_t<T>>)
                and (std::is_move_assignable_v<std::decay_t<T>> or std::is_move_constructible_v<std::decay_t<T>>);

    public:

        typedef std::conditional_t<is_rvalue_type, T &&, T&> type;
    };

    template<typename T>
    using argument_ref_t = argument_ref<T>::type;


    template <typename dataT, std::size_t alignmentV = 8>
    struct aligned_data {
        alignas(alignmentV) dataT _data;
    };

    // NOTE: Тип метаданных, для хранения рабочей информации непосредственно в сущности узла
    template<size_t DataSize = constants::word_size>
    struct alignas(constants::min_2_power(DataSize)) meta_data {
        [[nodiscard]] uint8_t &operator[](uint8_t idx) {
            return *((uint8_t *) this + (idx % 8));
        }

        // NOTE: Оператор присвоения данных к виду метаданных
        template<typename T>
        T &operator=(T data) requires (sizeof(T) <= std::bit_ceil(DataSize)) {
            return *reinterpret_cast<T *>(this) = data;
        }

        // NOTE: Преобразование метаданных под запрашиваемый тип
        template<typename T>
        T &release(T &target) {
            return target = *reinterpret_cast<T *>(this);
        }

        // NOTE: Преобразование метаданных под запрашиваемый тип
        template<typename T>
        T& release() { return *reinterpret_cast<T *>(this); }
    };


    // NOTE: Тип для хранения данных вместе с метаданными и внутренним выравниванием по машинному слову
    template<typename ChunkType, size_t MetaSize = constants::word_size>
    struct meta_chunk {
        meta_data<constants::min_2_power(MetaSize)>              _meta_data {};
        alignas(constants::word_alignment<ChunkType>) ChunkType  _mem       {};
    };

    // NOTE: Сигнатура функции аллокации памяти
    typedef void*(mem_alloc_t)(size_t);

    // NOTE: Сигнатура функции освобождения памяти
    typedef void(mem_free_t)(void*);

    #define NUKES_DEFAULT_ALLOCATE_FUNC malloc

    #define NUKES_DEFAULT_FREE_FUNC free

} // end namespace nukes


#endif // NUKES_MISC
