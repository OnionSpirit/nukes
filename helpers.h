#ifndef NUKES_HELPERS
#define NUKES_HELPERS

#include <atomic>
#include <bit>

#include "constants.h"



namespace nukes {


    template<typename NextType>
    struct atomic_typedef_mixin {
        typedef std::atomic<NextType> atomic_t;
    };


    consteval size_t min_2_power (size_t len) { return std::bit_ceil(len); }


    template<typename T>
    class fn_forward {

        static constexpr bool rvalue_assign_type =
                not std::is_copy_assignable_v<T>
                and std::is_move_assignable_v<T>;

        static constexpr bool is_short_type = sizeof(T) <= 8;

    public:

        typedef std::conditional_t<rvalue_assign_type, T &&,
                std::conditional_t<is_short_type, T, T &>
        > type;
    };

    template<typename T>
    using fn_forward_t = fn_forward<T>::type;


    template<size_t DataSize = constants::machine_word_size>
    struct alignas(min_2_power(DataSize)) meta_data {
        [[nodiscard]] uint8_t &operator[](uint8_t idx) {
            return *((uint8_t *) this + (idx % 8));
        }

        template<typename T>
        T &operator=(T data) requires (sizeof(T) <= std::bit_ceil(DataSize)) {
            return *reinterpret_cast<T *>(this) = data;
        }

        template<typename T>
        T &release(T &target) {
            return target = *reinterpret_cast<T *>(this);
        }
    };


    template<typename ChunkType, size_t MetaSize = constants::machine_word_size>
    struct meta_chunk {
        meta_data<min_2_power(MetaSize)>                        _meta_data {};
        alignas(constants::word_alignment<ChunkType>) ChunkType _mem       {};
    };


} // end namespace nukes


#endif // NUKES_HELPERS
