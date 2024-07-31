/**
 * @file
 * @details Contains atomic_mpmc_queue_base declaration
 */
#ifndef RIOT_NUKES_ATOMIC_MPMC_QUEUE_BASE
#define RIOT_NUKES_ATOMIC_MPMC_QUEUE_BASE


#include <atomic>

#include "helpers.h"
#include "meta.h"
#include "node_types.h"



namespace nukes {

    
/**
 * @details atomic_queue base class
 * @tparam Data Type that assumed to be used in the queue
 */
template <typename dataT>
struct atomic_queue_base {
    
protected:

    typedef dyn_node<dataT> node;                ///< Node type declaration
    
    std::atomic<dyn_node_hdl> _head { &_dummy }; ///< Head pointer
    std::atomic<dyn_node_hdl> _tail { &_dummy }; ///< Tail pointer

    node _dummy {};                              ///< Dummy node instance
    
    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param node Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(node*& n) noexcept;

public:
    
    atomic_queue_base() = default;

    virtual ~atomic_queue_base() = default;

    /**
     * @details Atomically pushes element to the queue with allocation
     * @param data Data to be pushed
     */
    void push_new(fn_forward_t<dataT> data) noexcept;

    /**
     * @details Atomically pops an element from the queue to the reference
     * from function arg, returns the result of operation and removes
     * node instance
     * @param data Reference to storage of pulled element
     * @return @b True if element successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] bool pop_new(dataT& data) noexcept;

    /**
     * @details Atomically pushes node instance to the queue (Move Semantics)
     * @param node Node instance to be pushed
     */
    void push_node(fn_forward_t<node> n) noexcept;

    /**
     * @details Atomically pops an node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] bool pop_node(node *&n) noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

 
} // end namespace nukes


// ================================ DEFINITIONS ================================


ATOMIC_MPMC_QUEUE_BASE_MEMBER(bool)
recycle_dummy(node*& n) noexcept {

    if (n == &_dummy) [[unlikely]] {
        n->_next.store(dyn_node_hdl{}, std::memory_order_release);
        dyn_node_hdl new_tail_hdl, tail_hdl = _tail.load(std::memory_order_acquire);
        new_tail_hdl._node = n;       
        while (not _tail.compare_exchange_weak(tail_hdl, new_tail_hdl, std::memory_order_release,
                                               std::memory_order_relaxed)) {}
        reinterpret_cast<node*>(tail_hdl._node)
            ->_next.store(new_tail_hdl,std::memory_order_release);
        return true;
    } else [[likely]] return false;
}


ATOMIC_MPMC_QUEUE_BASE_MEMBER(void)
push_new(fn_forward_t<dataT> data) noexcept {

    auto n = reinterpret_cast<node*>(malloc(sizeof(node)));
    n->_data = std::forward<dataT>(data);
    n->_next.store(dyn_node_hdl{}, std::memory_order_release);
    
    dyn_node_hdl new_tail_hdl, tail_hdl = _tail.load(std::memory_order_acquire);
    new_tail_hdl._node = n;       
    while (not _tail.compare_exchange_weak(tail_hdl, new_tail_hdl, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node*>(tail_hdl._node)
        ->_next.store(new_tail_hdl,std::memory_order_release);
}


ATOMIC_MPMC_QUEUE_BASE_MEMBER(bool)
pop_new(dataT &data) noexcept {

    while (true) {
        dyn_node_hdl new_head_hdl, head_hdl = _head.load(std::memory_order_acquire);

        do {if (not head_hdl._node) [[unlikely]] return false;
            new_head_hdl._tag = head_hdl._tag + 1;
            new_head_hdl._node = reinterpret_cast<node *>(head_hdl._node)->_next.load()._node;
            if (not new_head_hdl._node) [[unlikely]] return false;
        } while (not _head.compare_exchange_weak(head_hdl, new_head_hdl, std::memory_order_release,
                                                 std::memory_order_relaxed));

        auto* n = std::forward<node*>(reinterpret_cast<node*>(head_hdl._node));
    
        if (not recycle_dummy(n)) [[likely]] {
            data = std::forward<dataT>(n->_data);
            delete n;
            return true;
        }
    }
}


ATOMIC_MPMC_QUEUE_BASE_MEMBER(void)
push_node(fn_forward_t<node> n) noexcept {
    
    n->_next.store(dyn_node_hdl{}, std::memory_order_release);
    
    dyn_node_hdl new_tail_hdl, tail_hdl = _tail.load(std::memory_order_acquire);
    new_tail_hdl._node = n;       
    while (not _tail.compare_exchange_weak(tail_hdl, new_tail_hdl, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node*>(tail_hdl._node)
        ->_next.store(new_tail_hdl,std::memory_order_release);
}


ATOMIC_MPMC_QUEUE_BASE_MEMBER(bool)
pop_node(node*& n) noexcept {
    
    while (true) {
        dyn_node_hdl new_head_hdl, head_hdl = _head.load(std::memory_order_acquire);

        do {if (not head_hdl._node) [[unlikely]] return false;
            new_head_hdl._tag = head_hdl._tag + 1;
            new_head_hdl._node = reinterpret_cast<node *>(head_hdl._node)->_next.load()._node;
            if (not new_head_hdl._node) [[unlikely]] return false;
        } while (not _head.compare_exchange_weak(head_hdl, new_head_hdl, std::memory_order_release,
                                                 std::memory_order_relaxed));

        n = std::forward<node*>(reinterpret_cast<node*>(head_hdl._node));
    
        if (not recycle_dummy(n)) [[likely]] {
            return true;
        }
    }
}


ATOMIC_MPMC_QUEUE_BASE_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else return false;
}


#undef ATOMIC_MPMC_QUEUE_BASE_MEMBER
#endif // NUKES_ATOMIC_MPMC_QUEUE_BASE
