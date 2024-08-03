#ifndef NUKES_ATOMIC_UNBOUNDED_STACK
#define NUKES_ATOMIC_UNBOUNDED_STACK

#include <atomic>

#include "helpers.h"
#include "meta.h"
#include "node_types.h"
#include "atomic_unbounded_freelist.h"



namespace nukes {


template
<
    typename dataT,
    
    size_t BucketByteSize =
        constants::ufl_memory_offset
      + sizeof(meta_chunk<dataT>) * 64,

    void* (mem_alloc) (size_t) = malloc,
    
    void  (mem_free)  (void*)  = free
>
struct alignas(8) atomic_unbounded_stack {

protected:
    
    typedef dyn_node<dataT> node_t;

    std::atomic<dyn_node_hdl> _top {};
    atomic_unbounded_freelist<node_t, BucketByteSize,
                              mem_alloc, mem_free> _free_nodes {};

public:

    atomic_unbounded_stack() noexcept =default;

    ~atomic_unbounded_stack() noexcept =default;

    [[nodiscard]] bool push(fn_forward_t<dataT> data) noexcept;
        
    [[nodiscard]] bool pop(dataT& data) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================


ATOMIC_UNBOUNDED_STACK_MEMBER(bool)
push(fn_forward_t<dataT> data) noexcept {

    node_t* new_node { nullptr };
    const bool res = _free_nodes.capture(new_node);
    new_node->_data = std::forward<dataT>(data);

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    new_top_tag_hdl._node = new_node;
    
    do {
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_node->_next.store(top_tag_hdl);
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));
    
    return res;
}


ATOMIC_UNBOUNDED_STACK_MEMBER(bool)
pop(dataT &data) noexcept {

    dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();
    
    do {
        if (not top_tag_hdl._node) return false;
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_top_tag_hdl._node = ((node_t*)top_tag_hdl._node)->_next.load()._node;
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));

    data = ((node_t*)top_tag_hdl._node)->_data;
    return _free_nodes.sync((node_t*)top_tag_hdl._node);
}


#undef ATOMIC_UNBOUNDED_STACK_MEMBER
#endif // NUKES_ATOMIC_UNBOUNDED_STACK
