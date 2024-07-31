#ifndef NUKES_NODE_TYPES
#define NUKES_NODE_TYPES

#include "helpers.h"



namespace nukes {

    
struct alignas(8) dyn_node_hdl {
    void*    _node {nullptr};
    uint64_t _tag  {0};
};

    
struct alignas(8) stc_node_hdl {
    uint32_t _node_idx {UINT32_MAX};
    uint32_t _tag      {0};
};

    
template <typename dataT>
struct alignas(8) dyn_node
    : private atomic_typedef_mixin<dyn_node_hdl> {
    
    atomic_t _next {};
    dataT    _data {};
};

    
template <typename dataT>
struct alignas(8) stc_node
    : private atomic_typedef_mixin<stc_node_hdl> {
    
    atomic_t _next {};
    dataT    _data {};
};


template <typename ChunkType>
struct alignas(8) mem_node
    : private atomic_typedef_mixin<stc_node_hdl> {
    
    atomic_t  _next {};
    ChunkType _mem  {};
};

    
} // end namespace nukes

#endif // NUKES_NODE_TYPES

