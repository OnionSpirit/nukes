#ifndef NUKES_UNBOUNDED_ATOMIC_STACK
#define NUKES_UNBOUNDED_ATOMIC_STACK

#include <atomic>

#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "nukes/pool/atomic_freelist.h"


namespace nukes::unbounded {


template
<
    typename dataT,

    size_t BucketByteSize =
        details::constants::ufl_memory_offset
      + sizeof(details::misc::meta_chunk<details::nodes::dyn_node<dataT>, 8>) * 64,

    void* (mem_alloc) (size_t) = malloc,

    void  (mem_free)  (void*)  = free
>
struct atomic_stack {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;

    std::atomic<details::nodes::dyn_node_hdl> _top {};
    pool::atomic_freelist<node_t, BucketByteSize, pool::atomic_fifo_pool,
                              mem_alloc, mem_free> _free_nodes {};

public:

    atomic_stack() noexcept =default;

    ~atomic_stack() noexcept =default;

    [[nodiscard]] bool push(details::misc::fn_forward_t<dataT> data) noexcept;

    [[nodiscard]] bool pop(dataT& data) noexcept;
};


} // end namespace nukes::unbounded


// ================================ DEFINITIONS ================================

#define ATOMIC_STACK_MEMBER(member_type)                                         \
    template                                                                     \
    <                                                                            \
        typename dataT,                                                          \
        size_t bytes_per_bucketV,                                                \
        void* (mem_alloc) (size_t),                                              \
        void  (mem_free)  (void*)                                                \
    >                                                                            \
    member_type nukes::unbounded::atomic_stack<dataT, bytes_per_bucketV,         \
                                               mem_alloc, mem_free>::


ATOMIC_STACK_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {

    node_t* new_node { nullptr };
    const bool res = _free_nodes.capture(new_node);
    new_node->_data = std::forward<dataT>(data);

    details::nodes::dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();

    new_top_tag_hdl._node = new_node;

    do {
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_node->_next.store(top_tag_hdl);
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));

    return res;
}


ATOMIC_STACK_MEMBER(bool)
pop(dataT &data) noexcept {

    details::nodes::dyn_node_hdl new_top_tag_hdl, top_tag_hdl = _top.load();

    do {
        if (not top_tag_hdl._node) return false;
        new_top_tag_hdl._tag = top_tag_hdl._tag + 1;
        new_top_tag_hdl._node = ((node_t*)top_tag_hdl._node)->_next.load()._node;
    } while (not _top.compare_exchange_weak(top_tag_hdl, new_top_tag_hdl));

    data = ((node_t*)top_tag_hdl._node)->_data;
    return _free_nodes.sync((node_t*)top_tag_hdl._node);
}


#undef ATOMIC_STACK_MEMBER
#endif // NUKES_UNBOUNDED_ATOMIC_STACK
