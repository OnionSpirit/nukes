#ifndef NUKES_ATOMIC_BOUNDED_STACK
#define NUKES_ATOMIC_BOUNDED_STACK

#include <atomic>

#include "helpers.h"
#include "meta.h"
#include "node_types.h"
#include "atomic_bounded_freelist.h"



namespace nukes {


template <typename dataT, uint32_t ssize = 1024>
struct alignas(8) atomic_bounded_stack {

protected:
    
    typedef stc_node<dataT> node_t;

    std::atomic<stc_node_hdl> _top {};
    atomic_bounded_freelist<node_t, ssize> _free_nodes {};

public:

    atomic_bounded_stack() noexcept =default;

    ~atomic_bounded_stack() noexcept =default;

    bool push(fn_forward_t<dataT> data) noexcept;
        
    bool pop(dataT& data) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================


ATOMIC_BOUNDED_STACK_MEMBER(bool)
push(fn_forward_t<dataT> data) noexcept {

    node_t* new_node {nullptr};
    _free_nodes.capture(new_node);
    if (not new_node) [[unlikely]] return false;
    
    new_node->_data = std::forward<dataT>(data);
    stc_node_hdl new_top_hdl, top_hdl = _top.load();      
    new_top_hdl._node_idx = _free_nodes.idx_by_ptr(new_node);
    
    do {
        new_top_hdl._tag = top_hdl._tag + 1;
        new_node->_next.store(top_hdl);
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));
    
    return true;
}


ATOMIC_BOUNDED_STACK_MEMBER(bool)
pop(dataT &data) noexcept {

    stc_node_hdl new_top_hdl, top_hdl = _top.load();
    node_t* node { nullptr };
    
    do { if (top_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;

        node
            = _free_nodes.ptr_by_idx(top_hdl._node_idx);
        new_top_hdl._tag
            = top_hdl._tag + 1;
        new_top_hdl._node_idx
            = node->_next.load()._node_idx;
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    data = node->_data;
    _free_nodes.sync_idx(top_hdl._node_idx);
    return true;
}


#undef ATOMIC_BOUNDED_STACK_MEMBER
#endif // NUKES_ATOMIC_BOUNDED_STACK
