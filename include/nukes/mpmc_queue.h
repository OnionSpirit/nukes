/**
 * @file
 * @details Contains atomic_mpmc_queue_base declaration
 */
#ifndef NUKES_MPMC_QUEUE
#define NUKES_MPMC_QUEUE


#include <atomic>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
// #include "nukes/details/batch.h"
#include "nukes/memory/atomic_bucketlist.h"


namespace nukes {


/**
 * @details atomic mpmc queue base class
 * @tparam dataT Type that assumed to be used in the queue
 * @tparam capacityV Value of capacity that will affect mempool object size
 * @tparam mempoolT Type of buffer that will be used as allocator for nodes
 */
template <
    typename dataT,

    nukes::details::constants::hword capacityV = details::constants::runtime_discover,

    template <typename, nukes::details::constants::hword> typename mempoolT = memory::atomic_bucketlist
>
struct mpmc_queue {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;  ///< Node type declaration
    typedef mempoolT<node_t, capacityV> mempool_t;      ///< Memory buffer type


    mempool_t _mempool {};  ///< Memory buffer to allocate nodes from
    node_t    _dummy {};    ///< Dummy node instance
    alignas(64) std::atomic<node_t*> _head { &_dummy }; ///< Head pointer
    alignas(64) std::atomic<node_t*> _tail { &_dummy }; ///< Tail pointer

    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param dummy Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(node_t*& dummy) noexcept;

public:

    explicit mpmc_queue(nukes::details::constants::hword l = 1024) noexcept
    requires (capacityV == details::constants::runtime_discover): _mempool { mempool_t(l) }
    { };

    mpmc_queue() noexcept
    requires (capacityV != details::constants::runtime_discover) : _mempool { mempool_t() }
    { };

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
    void push_node(details::misc::fn_forward_t<node_t> node) noexcept;

    /**
     * @details Atomically releases node to the queue mempool (Move Semantics)
     * @param node Releasing node
     */
    void release_node(details::misc::fn_forward_t<node_t> node) noexcept;

    /**
     * @details Atomically pops a node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] bool pop_node(node_t *& node) noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

template<typename dataT, size_t capacityV = details::constants::runtime_discover>
using bounded_mpmc_queue = mpmc_queue<dataT, capacityV, memory::atomic_fifo>;

template<typename dataT, size_t capacityV = details::constants::runtime_discover>
using bounded_mpmc_queue_lifo_pool = mpmc_queue<dataT, capacityV, memory::atomic_lifo>;

} // end namespace nukes


// ================================ DEFINITIONS ================================


#define MPMC_QUEUE_MEMBER(member_type)                                             \
    template <typename dataT,                                                      \
              nukes::details::constants::hword capacityV,                          \
              template <typename, nukes::details::constants::hword> typename poolT \
              >                                                                    \
    member_type nukes::mpmc_queue <                                                \
                dataT, capacityV, poolT>::


MPMC_QUEUE_MEMBER(bool)
recycle_dummy(node_t*& dummy) noexcept {

    if (dummy == &_dummy) [[unlikely]] {
        dummy->_next.store(nullptr, std::memory_order_release);
        node_t* current_tail = _tail.load(std::memory_order_acquire);
        while (not _tail.compare_exchange_weak(current_tail, dummy, std::memory_order_release,
                                               std::memory_order_relaxed)) {}
        current_tail->_next.store(dummy,std::memory_order_release);
        return true;
    } else [[likely]] return false;
}


MPMC_QUEUE_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {

    node_t* new_tail { nullptr };
    if (not _mempool.capture(new_tail)) return false;
    new_tail->_data = std::forward<dataT>(data);
    new_tail->_next.store(nullptr, std::memory_order_release);

    node_t *current_tail = _tail.load(std::memory_order_acquire);
    while (not _tail.compare_exchange_weak(current_tail, new_tail
                                           , std::memory_order_release
                                           , std::memory_order_relaxed)) {}
    current_tail->_next.store(new_tail,std::memory_order_release);

    return true;
}


MPMC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    while (true) {
        node_t *new_head, *current_head = _head.load(std::memory_order_acquire);

        do {if (not current_head) [[unlikely]] return false;
            new_head = reinterpret_cast<node_t *>(current_head->_next.load());
            if (not new_head) [[unlikely]] return false;
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        auto* pop_node = std::forward<node_t*>(current_head);
        if (not recycle_dummy(pop_node)) [[likely]] {
            data = std::forward<dataT>(pop_node->_data);
            return _mempool.sync(pop_node);
        }
    }
}


MPMC_QUEUE_MEMBER(void)
push_node(details::misc::fn_forward_t<node_t> node) noexcept {

    node->_next.store(node_t{}, std::memory_order_release);

    node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
    new_tail = node;
    while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node_t*>(current_tail)
        ->_next.store(new_tail,std::memory_order_release);
}


MPMC_QUEUE_MEMBER(void)
release_node(details::misc::fn_forward_t<node_t> node) noexcept {
    return _mempool.sync(node);
}


MPMC_QUEUE_MEMBER(bool)
pop_node(node_t*& node) noexcept {

    while (true) {
        node_t new_head, current_head = _head.load(std::memory_order_acquire);

        do {if (not current_head) [[unlikely]] return false;
            new_head = reinterpret_cast<node_t *>(current_head)->_next.load()._node;
            if (not new_head) [[unlikely]] return false;
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        node = std::forward<node_t*>(reinterpret_cast<node_t*>(current_head));

        if (not recycle_dummy(node)) [[likely]] {
            return true;
        }
    }
}


MPMC_QUEUE_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else return false;
}


#undef MPMC_QUEUE_MEMBER
#endif // NUKES_MPMC_QUEUE
