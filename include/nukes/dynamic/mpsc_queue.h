/**
 * @file
 * @details Contains atomic_mpsc_queue_base declaration
 */
#ifndef NUKES_DYNAMIC_MPSC_QUEUE
#define NUKES_DYNAMIC_MPSC_QUEUE


#include <atomic>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "atomic_freelist.h"


namespace nukes::dynamic {


/**
 * @details atomic mpsc queue base class, can be used as dyn spsc queue because they have same realisation
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct mpsc_queue {

    typedef details::nodes::dyn_node<dataT> node_t;      ///< Node type declaration
    typedef atomic_freelist<node_t> mempool_t;   ///< Memory buffer type

protected:

    node_t    _dummy {};    ///< Dummy node instance

    alignas(64) node_t*              _head { &_dummy };  ///< Head pointer
    alignas(64) std::atomic<node_t*> _tail { &_dummy };  ///< Tail pointer

    mempool_t _mempool {};  ///< Memory buffer to allocate nodes from

    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param node Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(details::misc::argument_ref_t<node_t*> node) noexcept;

public:

    mpsc_queue() noexcept = default;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool push(dataT&& data) noexcept;

    /**
     * @details Atomically pops an element from the queue to the reference
     * from function arg, returns the result of operation
     * @param data Reference to storage of pulled element
     * @return @b True if element successfully pulled,
     * @b False when pulling failed or data node sync failed
     */
    [[nodiscard]] bool pop(dataT& data) noexcept;

    /**
     * @details Atomically pushes node instance to the queue (Move Semantics)
     * @param node Node instance to be pushed
     */
    void push_node(details::misc::argument_ref_t<node_t*> node) noexcept;

    /**
     * @details Atomically releases node to the queue mempool (Move Semantics)
     * @param node Releasing node
     */
    void release_node(details::misc::argument_ref_t<node_t*> node) noexcept;

    /**
     * @details Atomically pops an node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] node_t* pop_node() noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

} // end namespace nukes::dynamic


// ================================ DEFINITIONS ================================


#define DYNAMIC_MPSC_QUEUE_MEMBER(member_type)         \
    template <typename dataT>                          \
        member_type nukes::dynamic::mpsc_queue<dataT>::


DYNAMIC_MPSC_QUEUE_MEMBER(bool)
push(dataT&& data) noexcept {

    node_t* new_node { nullptr };
    if (not _mempool.capture(new_node)) return false;
    new_node->_data = std::forward<dataT>(data);

    push_node(new_node);

    return true;
}


DYNAMIC_MPSC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    auto* released_node = pop_node();
    if (not released_node) [[unlikely]] return false;

    data = std::forward<dataT>(released_node->_data);
    return _mempool.sync(released_node);
}


DYNAMIC_MPSC_QUEUE_MEMBER(void)
release_node(details::misc::argument_ref_t<node_t*> node) noexcept {
    _mempool.sync(node);
}


DYNAMIC_MPSC_QUEUE_MEMBER(bool)
recycle_dummy(details::misc::argument_ref_t<node_t*> dummy) noexcept {

    if (dummy == &_dummy) [[unlikely]] {
        dummy->_next.store(nullptr, std::memory_order_release);
        node_t* current_tail = _tail.load(std::memory_order_acquire);
        while (not _tail.compare_exchange_weak(current_tail, dummy, std::memory_order_release,
                                               std::memory_order_relaxed)) {}
        current_tail->_next.store(dummy,std::memory_order_release);
        return true;
    } else [[likely]] return false;
}


DYNAMIC_MPSC_QUEUE_MEMBER(void)
push_node(details::misc::argument_ref_t<node_t*> node) noexcept {

    node->_next.store(nullptr, std::memory_order_release);
    node_t* current_tail = _tail.load(std::memory_order_acquire);
    while (not _tail.compare_exchange_weak(current_tail, node, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    current_tail->_next.store(std::forward<node_t*>(node), std::memory_order_release);
}


DYNAMIC_MPSC_QUEUE_MEMBER(auto)
pop_node() noexcept -> node_t* {
    node_t* released_node;
    do {
        auto* new_head = reinterpret_cast<node_t *>(_head->_next.load());
        if (nullptr == new_head) [[unlikely]] return nullptr;
        released_node = std::forward<node_t*>(_head);
        _head = std::forward<node_t*>(new_head);
    } while (recycle_dummy(released_node));

    return std::forward<node_t*>(released_node);
}

DYNAMIC_MPSC_QUEUE_MEMBER(bool)
empty() noexcept {

    if (_head == _tail.load(std::memory_order_acquire))
        return true;
    else return false;
}



#undef DYNAMIC_MPSC_QUEUE_MEMBER
#endif // NUKES_DYNAMIC_MPSC_QUEUE
