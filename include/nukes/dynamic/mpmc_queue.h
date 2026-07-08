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
#include "mpmc_freelist.h"


namespace nukes::dynamic {


/**
 * @details atomic mpmc queue base class
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct mpmc_queue {

    typedef details::nodes::dyn_node<dataT> node_t;  ///< Node type declaration
    typedef mpmc_freelist<node_t> mempool_t;         ///< Memory buffer type

private:

    class dyn_mpmc_iter {

        mpmc_queue* _queue { nullptr };

    public:
        explicit dyn_mpmc_iter(mpmc_queue* queue)
            : _queue(queue) {}

        dyn_mpmc_iter& prefix_increment(node_t*& ptr) {
            node_t* new_ptr = ptr->next();
            if (node_t* new_new_ptr = new_ptr->next(); _queue->recycle_dummy(new_ptr))
                new_ptr = new_new_ptr;
            _queue->_mempool.sync(ptr);
            ptr = new_ptr;
            return *this;
        }

        dyn_mpmc_iter postfix_increment(node_t*& ptr)  {
            dyn_mpmc_iter tmp = *this;
            node_t* new_ptr = ptr->next();
            if (node_t* new_new_ptr = new_ptr->next(); _queue->recycle_dummy(new_ptr))
                new_ptr = new_new_ptr;
            _queue->_mempool.sync(ptr);
            ptr = new_ptr;
            return tmp;
        }
    };

    typedef details::batch<node_t, dyn_mpmc_iter, mpmc_queue*> batch_t;

    alignas(64) std::atomic<node_t*> _head     ; ///< Head pointer
    alignas(64) std::atomic<node_t*> _tail     ; ///< Tail pointer
    node_t*                          _dummy_ptr; ///< Dummy helper node

    mempool_t          _mempool   { };  ///< Memory buffer to allocate nodes from

    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param dummy Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(details::misc::argument_ref_t<node_t*> dummy) noexcept;

public:

    mpmc_queue() noexcept {
        if (not _mempool.capture(_dummy_ptr)) return;
        _head = _dummy_ptr;
        _tail.store(_dummy_ptr, std::memory_order_relaxed);
    }

    mpmc_queue(mpmc_queue&& q)  noexcept {
        this->_head = q._head.load(std::memory_order_relaxed);
        this->_tail = q._tail.load(std::memory_order_relaxed);
        this->_mempool = std::forward<mempool_t>(q._mempool);
        this->_dummy_ptr = q._dummy_ptr;
    }

    mpmc_queue(const mpmc_queue&) = delete;

    mpmc_queue operator=(const mpmc_queue&) = delete;

    mpmc_queue& operator=(mpmc_queue&& q)  noexcept {
        this->_head = q._head.load(std::memory_order_relaxed);
        this->_tail = q._tail.load(std::memory_order_relaxed);
        this->_mempool = std::forward<mempool_t>(q._mempool);
        this->_dummy_ptr = q._dummy_ptr;
        return *this;
    }

    ~mpmc_queue() noexcept = default;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool push(details::misc::argument_t<dataT> data) noexcept;

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
    void push_node(details::misc::argument_ref_t<node_t*> node) noexcept;

    /**
     * @details Atomically releases node to the queue mempool (Move Semantics)
     * @param node Releasing node
     */
    void release_node(details::misc::argument_ref_t<node_t*> node) noexcept;

    /**
     * @details Atomically pops a node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] node_t* pop_node() noexcept;

    /**
     * @details Извлекает всю очередь атомарно, в формате листа
     * @return Объект @b batch содержащий только операции получения итератора,
     * для прохода по списку
     */
    batch_t pop_batch() noexcept {
        node_t *current_head = _head.load(std::memory_order_acquire);
        node_t *current_tail = _tail.load(std::memory_order_acquire);
            do {
                if (not current_head or not current_tail) [[unlikely]] return batch_t{};
            } while (not _head.compare_exchange_strong(current_head, current_tail,
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed));
        return batch_t { current_head, current_tail, this };
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
recycle_dummy(details::misc::argument_ref_t<node_t*> dummy) noexcept {

    if (dummy == _dummy_ptr) [[unlikely]] {
        dummy->_next.store(nullptr, std::memory_order_release);
        node_t *current_tail = _tail.exchange(dummy, std::memory_order_release);
        current_tail->_next.store(dummy,std::memory_order_release);
        return true;
    }
    return false;
}


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
push(details::misc::argument_t<dataT> data) noexcept {

    node_t* new_node { nullptr };
    if (not _mempool.capture(new_node)) return false;
    new_node->_data = std::forward<dataT>(data);

    push_node(new_node);

    return true;
}


DYNAMIC_MPMC_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    auto* released_node = pop_node();
    if (not released_node) [[unlikely]] return false;

    data = std::forward<dataT>(released_node->_data);
    return _mempool.sync(released_node);
}


DYNAMIC_MPMC_QUEUE_MEMBER(void)
push_node(details::misc::argument_ref_t<node_t*> node) noexcept {

    node->_next.store(nullptr, std::memory_order_release);
    node_t* current_tail = _tail.exchange(node, std::memory_order_release);
    current_tail->_next.store(std::forward<node_t*>(node), std::memory_order_release);
}


DYNAMIC_MPMC_QUEUE_MEMBER(void)
release_node(details::misc::argument_ref_t<node_t*> node) noexcept {
    _mempool.sync(node);
}


DYNAMIC_MPMC_QUEUE_MEMBER(auto)
pop_node() noexcept -> node_t* {
    node_t *new_head, *current_head;
    do {
        current_head = _head.load(std::memory_order_acquire);
        do {
            cxhg_loop:
            if (not current_head) [[unlikely]] return nullptr;
            new_head = reinterpret_cast<node_t*>(current_head->_next.load());
            if (not new_head) [[unlikely]] return nullptr;
            if (node_t* actual_head = _head.load(std::memory_order_acquire);
                actual_head not_eq current_head) [[unlikely]] {
                current_head = actual_head;
                goto cxhg_loop;
            }
        } while (not _head.compare_exchange_weak(current_head, new_head,
                                                 std::memory_order_release,
                                                 std::memory_order_relaxed));
    } while(recycle_dummy(current_head));
    return std::forward<node_t*>(current_head);
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
