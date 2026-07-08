#ifndef NUKES_NODE_TYPES
#define NUKES_NODE_TYPES

#include "misc.h"
#include "constants.h"



namespace nukes::details::nodes {

// NOTE: Тип управляющего заголовка узла для структур с конечным количеством узлов
struct alignas(constants::hword_size) stc_node_hdr {
    constants::hword _node_idx {constants::hword_max_v};
    constants::hword _tag      {constants::hword_min_v};
};


// NOTE: Тип управляющего заголовка узла для структур с конечным количеством узлов
template <typename dataT>
struct stc_node
    : private misc::atomic_typedef_mixin<stc_node_hdr> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data;
};


// NOTE: Тип управляющего заголовка узла для структур с динамическим хранением узлов
template <typename dataT>
struct dyn_node
    : private misc::atomic_typedef_mixin<void*> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data;

    dyn_node* next() { return static_cast<dyn_node*>(_next.load()); }
};

// NOTE: Тип управляющего заголовка узла для неатомарных структур с динамическим хранением узлов
template <typename dataT>
struct dyn_reg_node {

    dyn_reg_node*                                      _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data;

    dyn_reg_node*& next() { return _next; }
};

// NOTE: Casting node from regular queue node to atomic queue
template <template <typename> typename node_t, typename dataT>
auto cast_node(dyn_reg_node<dataT>* node) noexcept {
    return reinterpret_cast<node_t<dataT>*>(node);
}

// NOTE: Casting node from atomic queue to regular queue node
template <template <typename> typename node_t, typename dataT>
auto cast_node(dyn_node<dataT>* node) noexcept {
    return reinterpret_cast<node_t<dataT>*>(node);
}

// NOTE: Тип узла для структур хранения зарезервированной памяти
template <typename ChunkType>
struct mem_node
    : private misc::atomic_typedef_mixin<stc_node_hdr> {

    atomic_t                                                _next {};
    alignas(constants::word_size) ChunkType                 _mem;
    // alignas(constants::word_alignment<ChunkType>) ChunkType _mem  {};
};


} // end namespace nukes


#endif // NUKES_NODE_TYPES
