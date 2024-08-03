#ifndef NUKES_HELPERS
#define NUKES_HELPERS

#include <atomic>



namespace nukes {


template <typename NextType>
struct atomic_typedef_mixin { typedef std::atomic<NextType> atomic_t; };


template <typename T>
class fn_forward {
    
    static constexpr bool rvalue_assign_type =
        not std::is_copy_assignable_v<T>
        and std::is_move_assignable_v<T>;

    static constexpr bool is_short_type = sizeof(T) <= 8;

    T _data {};
    
public:
    
    constexpr fn_forward(T& data) requires (not rvalue_assign_type) : _data{data} {};
    constexpr fn_forward(T&& data) requires (rvalue_assign_type) : _data{std::forward<T>(data)} {};
    
    typedef std::conditional_t<rvalue_assign_type, T&&,
        std::conditional_t<is_short_type, T, T&>
    > type;
};

template <typename T>
using fn_forward_t = fn_forward<T>::type;


template <size_t DataSize = 8UL> requires (0 == DataSize % 8)
struct alignas(DataSize) meta_data {
    [[nodiscard]] uint8_t& operator [](uint8_t idx) {
        return *((uint8_t*)this + (idx % 8));
    }

    template <typename T>
    T& operator =(T data) requires (sizeof(T) <= DataSize) {
        return *reinterpret_cast<T*>(this) = data;
    }

    template <typename T>
    T& release(T& target) {
        return target = *reinterpret_cast<T*>(this);
    }
};


template <typename ChunkType, size_t MetaSize = 8>    
struct alignas(8) meta_chunk {
    meta_data<MetaSize> _meta_data;
    ChunkType   _mem;
};


} // end namespace nukes

#endif // NUKES_HELPERS
