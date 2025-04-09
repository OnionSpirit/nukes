#ifndef NUKES_NODE_TYPES
#define NUKES_NODE_TYPES

#include "misc.h"
#include "constants.h"



namespace nukes::details::nodes {

// NOTE: Тип управляющего заголовка узла для структур с конечным колличеством узлов
struct alignas(constants::hword_size) stc_node_hdl {
    constants::hword _node_idx {constants::hword_max_v};
    constants::hword _tag      {constants::hword_min_v};
};


// NOTE: Тип управляющего заголовка узла для структур с динамическим хранением узлов
template <typename dataT>
struct dyn_node
    : private misc::atomic_typedef_mixin<void*> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data {};
};


// NOTE: Тип управляющего заголовка узла для структур с конечным колличеством узлов
template <typename dataT>
struct stc_node
    : private misc::atomic_typedef_mixin<stc_node_hdl> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data {};
};


// NOTE: Тип узла для структур хранения зарезервированной памяти
template <typename ChunkType>
struct mem_node
    : private misc::atomic_typedef_mixin<stc_node_hdl> {

    using typename misc::atomic_typedef_mixin<stc_node_hdl>::atomic_t;

    atomic_t                                                _next {};
    alignas(constants::word_size) ChunkType                 _mem  {};
    // alignas(constants::word_alignment<ChunkType>) ChunkType _mem  {};
};


} // end namespace nukes


#endif // NUKES_NODE_TYPES
