/**
 * @file
 * @details Contains atomic_mpsc_queue_base declaration
 */
#ifndef NUKES_MPSC_QUEUE
#define NUKES_MPSC_QUEUE


#include <atomic>
#include <cstddef>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "nukes/memory/atomic_bucketlist.h"


namespace nukes {


/**
 * @details atomic mpsc queue base class
 * @tparam dataT Type that assumed to be used in the queue
 * @tparam capacityV Value of capacity that will affect mempool object size
 * @tparam bufferT Type of buffer that will be used as allocator for nodes
 */
template <
    typename dataT,

    size_t capacityV = 16,

    template <typename, size_t> typename mempoolT = memory::atomic_bucketlist
>
struct mpsc_queue {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;  ///< Node type declaration
    typedef mempoolT<node_t, capacityV> mempool_t;   ///< Memory buffer type

    mempool_t _mempool {};  ///< Memory buffer to allocate nodes from
    node_t    _dummy {};    ///< Dummy node instance

    alignas(64) node_t*              _head { &_dummy }; ///< Head pointer
    alignas(64) std::atomic<node_t*> _tail { &_dummy };          ///< Tail pointer

    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param node Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(node_t*& n) noexcept;

public:

    mpsc_queue() noexcept = default;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool push(details::misc::fn_forward_t<dataT> data) noexcept;

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
    void push_node(details::misc::fn_forward_t<node_t> n) noexcept;

    /**
     * @details Atomically releases node to the queue mempool (Move Semantics)
     * @param node Releasing node
     */
    void release_node(details::misc::fn_forward_t<node_t> n) noexcept;

    /**
     * @details Atomically pops an node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] bool pop_node(node_t *&n) noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

template<typename dataT, size_t capacityV = details::constants::runtime_discover>
using bounded_mpsc_queue = mpsc_queue<dataT, capacityV, memory::atomic_lifo>;

template<typename dataT, size_t capacityV = details::constants::runtime_discover>
using bounded_mpsc_queue_fifo_pool = mpsc_queue<dataT, capacityV, memory::atomic_fifo>;

} // end namespace nukes


// ================================ DEFINITIONS ================================

#define MPSC_QUEUE_MEMBER(member_type)         \
    template <typename dataT,                       \
        size_t capacityV,                           \
        template <typename, size_t> typename poolT  \
        >                                           \
        member_type nukes::mpsc_queue <        \
        dataT, capacityV, poolT>::

MPSC_QUEUE_MEMBER(bool)
recycle_dummy(node_t*& n) noexcept {

    if (n == &_dummy) [[unlikely]] {
        n->_next.store(node_t{}, std::memory_order_release);
        node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
        new_tail = n;
        while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                               std::memory_order_relaxed)) {}
        reinterpret_cast<node_t*>(current_tail)
            ->_next.store(new_tail,std::memory_order_release);
        return true;
    } else [[likely]] return false;
}


MPSC_QUEUE_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {

    node_t* new_node { nullptr };
    const bool res = _mempool.capture(new_node);
    if (not res) return false;
    new_node->_data = std::forward<dataT>(data);
    new_node->_next.store(node_t{}, std::memory_order_release);

    node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
    new_tail = new_node;
    while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node_t*>(current_tail)
        ->_next.store(new_tail,std::memory_order_release);

    return true;
}


MPSC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {
    do {
        const node_t* new_head = _head->_next.load();
        if (nullptr == new_head) [[unlikely]] return false;
        else [[likely]] {
            data = std::forward<dataT>(_head->_data);
            _head = new_head;
        }
    } while (recycle_dummy(_head));

    return _mempool.sync(_head);
}


MPSC_QUEUE_MEMBER(void)
push_node(details::misc::fn_forward_t<node_t> node) noexcept {

    node->_next.store(node_t{}, std::memory_order_release);

    node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
    new_tail = node;
    while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node_t*>(current_tail)
        ->_next.store(new_tail,std::memory_order_release);
}


MPSC_QUEUE_MEMBER(void)
release_node(details::misc::fn_forward_t<node_t> node) noexcept {
    return _mempool.sync(node);
}


MPSC_QUEUE_MEMBER(bool)
pop_node(node_t*& n) noexcept {

    do {
        const node_t* new_head = _head->_next.load();
        if (nullptr == new_head) [[unlikely]] return false;
        else [[likely]] n = _head;
    } while (recycle_dummy(_head));

    return true;
}

MPSC_QUEUE_MEMBER(bool)
empty() noexcept {

    if (_head == _tail.load(std::memory_order_acquire)._node)
        return true;
    else return false;
}



#undef MPSC_QUEUE_MEMBER
#endif // NUKES_MPSC_QUEUE
