/**
 * @file
 * @details Contains atomic_mpmc_queue_base declaration
 */
#ifndef NUKES_DYNAMIC_MPMC_QUEUE
#define NUKES_DYNAMIC_MPMC_QUEUE


#include <atomic>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "nukes/details/batch.h"
#include "nukes/atomic_freelist.h"


namespace nukes::dynamic {


/**
 * @details atomic mpmc queue base class
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct mpmc_queue {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;     ///< Node type declaration
    typedef memory::atomic_freelist<node_t> mempool_t;  ///< Memory buffer type


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

    mpmc_queue() noexcept = default;

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
    bool pop(dataT& data) noexcept;

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
    bool pop_node(node_t *& node) noexcept;

    /**
     * @details Извлекает всю очередь атомарно, в формате листа
     * @return Объект @b batch содержащий только операции получения итератора,
     * для прохода по списку
     */
    details::batch<node_t, mempool_t> pop_batch() noexcept {
        typedef details::batch<node_t, mempool_t> batch_t;
        node_t *current_head = _head.load(std::memory_order_acquire);
        node_t *current_tail;
            do {
                current_tail = _tail.load(std::memory_order_acquire);
                if (not current_head) [[unlikely]] return batch_t{};
                if (not current_tail) [[unlikely]] return batch_t{};
            } while (not _head.compare_exchange_weak(current_head, current_tail,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed));
        return batch_t { current_head, current_tail, &_mempool };
    }

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    bool empty() noexcept;
};

} // end namespace nukes::dynamic


// ================================ DEFINITIONS ================================


#define DYNAMIC_MPMC_QUEUE_MEMBER(member_type)          \
    template <typename dataT>                           \
    member_type nukes::dynamic::mpmc_queue <dataT>::


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
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


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
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


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    while (true) {
        node_t *new_head, *current_head = _head.load(std::memory_order_acquire);

        do {
            // // NOTE: Делаем через goto, потому что continue станет проверять условие
            // //       с CAS операцией, а нам такого не надо
            // loop_begin:
            if (not current_head) [[unlikely]] return false;
            new_head = reinterpret_cast<node_t *>(current_head->_next.load());
            if (not new_head) [[unlikely]] return false;
            // if (_head.load(std::memory_order_relaxed) != current_head) [[unlikely]] {
            //     current_head = _head.load(std::memory_order_relaxed);
            //     goto loop_begin;
            // }
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        auto* pop_node = std::forward<node_t*>(current_head);
        if (not recycle_dummy(pop_node)) [[likely]] {
            data = std::forward<dataT>(pop_node->_data);
            return _mempool.sync(pop_node);
        }
    }
}


DYNAMIC_MPMC_QUEUE_MEMBER(void)
push_node(details::misc::fn_forward_t<node_t> node) noexcept {

    node->_next.store(node_t{}, std::memory_order_release);

    node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
    new_tail = node;
    while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node_t*>(current_tail)
        ->_next.store(new_tail,std::memory_order_release);
}


DYNAMIC_MPMC_QUEUE_MEMBER(void)
release_node(details::misc::fn_forward_t<node_t> node) noexcept {
    return _mempool.sync(node);
}


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
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


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else return false;
}


#undef DYNAMIC_MPMC_QUEUE_MEMBER
#endif // NUKES_DYNAMIC_MPMC_QUEUE
