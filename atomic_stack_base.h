#ifndef NUKES_ATOMIC_STACK_BASE
#define NUKES_ATOMIC_STACK_BASE

#include <atomic>

#include "helpers.h"
#include "meta.h"
#include "node_types.h"



namespace nukes {


template <typename dataT>
struct alignas(8) atomic_stack_base {

protected:

    typedef dyn_node<dataT> node_t;
    
    std::atomic<dyn_node_hdl> _top {};

public:
    
    [[nodiscard]] bool push_new(fn_forward_t<dataT> data);
        
    [[nodiscard]] bool pop_new(dataT& data);
    
    [[nodiscard]] bool push_node(fn_forward_t<node_t> node) noexcept;
        
    [[nodiscard]] bool pop_node(node_t& node) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================


ATOMIC_STACK_BASE_MEMBER(bool)
push_new(fn_forward_t<dataT> data) {

    node_t* new_node = new node_t();
    new_node->_data = std::forward<dataT>(data);

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    new_top_tag_hdl._node = new_node;
    
    do {
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_node->_next.store(top_tag_hdl);
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));
    
    return true;
}


ATOMIC_STACK_BASE_MEMBER(bool)
pop_new(dataT &data) {

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    do {
        if (not top_tag_hdl._node) return false;
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_top_tag_hdl._node = ((node_t*)top_tag_hdl._node)->_next.load()._node;
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));

    data = ((node_t*)top_tag_hdl._node)->_data;
    delete ((node_t*)top_tag_hdl._node);
    return true;
}


ATOMIC_STACK_BASE_MEMBER(bool)
push_node(fn_forward_t<node_t> node) noexcept {

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    new_top_tag_hdl._node = &node;
    
    do {
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        node._next.store(top_tag_hdl);
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));
    
    return true;
}


ATOMIC_STACK_BASE_MEMBER(bool)
pop_node(node_t &node) noexcept {

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    do {
        if (not top_tag_hdl._node) return false;
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_top_tag_hdl._node = ((node_t*)top_tag_hdl._node)->_next.load()._node;
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));

    node = *((node_t*)top_tag_hdl._node);
    return true;
}


#undef ATOMIC_STACK_BASE_MEMBER
#endif // NUKES_ATOMIC_STACK_BASE
