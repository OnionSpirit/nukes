#ifndef NUKES_ATOMIC_STACK
#define NUKES_ATOMIC_STACK

#include <atomic>

#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "../dynamic/mpmc_freelist.h"


namespace nukes {


template <typename dataT, uint32_t ssize = 1024>
struct atomic_stack {

protected:

    typedef details::nodes::stc_node<dataT> node_t;

    std::atomic<details::nodes::stc_node_hdr> _top {};      // NOTE: Квази-указатель вершины
    dynamic::mpmc_freelist<node_t, ssize> _free_nodes {};   // NOTE: pool аллокатор для хранения памяти под узлы

public:

    atomic_stack() noexcept =default;

    ~atomic_stack() noexcept =default;

    [[nodiscard]] bool push(details::misc::argument_t<dataT> data) noexcept;

    [[nodiscard]] bool pop(dataT& data) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_STACK_MEMBER(member_type)            \
    template <typename dataT, uint32_t ssize>       \
    member_type nukes::atomic_stack<dataT, ssize>::


ATOMIC_STACK_MEMBER(bool)
push(details::misc::argument_t<dataT> data) noexcept {

    node_t* new_node {nullptr};
    const bool res = _free_nodes.capture(new_node);
    if (not new_node) [[unlikely]] return false;

    new_node->_data = std::forward<dataT>(data);
    details::nodes::stc_node_hdr new_top_hdl, top_hdl = _top.load();
    new_top_hdl._node_idx = _free_nodes.idx_by_ptr(new_node);

    do {
        new_top_hdl._tag = top_hdl._tag + 1;
        new_node->_next.store(top_hdl);
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    return res;
}


ATOMIC_STACK_MEMBER(bool)
pop(dataT &data) noexcept {

    details::nodes::stc_node_hdr new_top_hdl, top_hdl = _top.load();
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
    return _free_nodes.sync_idx(top_hdl._node_idx);
}


#undef ATOMIC_STACK_MEMBER
#endif // NUKES_ATOMIC_STACK
