#ifndef NUKES_NODE_TYPES
#define NUKES_NODE_TYPES

#include "helpers.h"
#include "constants.h"



namespace nukes {


struct alignas(8) dyn_node_hdl {
    void*    _node {nullptr};
    size_t   _tag  {0};
};


struct alignas(4) stc_node_hdl {
    uint32_t _node_idx {UINT32_MAX};
    uint32_t _tag      {0};
};


template <typename dataT>
struct dyn_node
    : public atomic_typedef_mixin<dyn_node_hdl> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data {};
};


template <typename dataT>
struct stc_node
    : private atomic_typedef_mixin<stc_node_hdl> {

    atomic_t                                           _next {};
    alignas(constants::word_alignment<dataT>) dataT    _data {};
};


template <typename ChunkType>
struct mem_node
    : private atomic_typedef_mixin<stc_node_hdl> {

    atomic_t                                                _next {};
    alignas(constants::word_alignment<ChunkType>) ChunkType _mem  {};
};


} // end namespace nukes


#endif // NUKES_NODE_TYPES

