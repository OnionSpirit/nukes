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
#include "spmc_freelist.h"
#include "nukes/details/batch.h"
#include "nukes/details/prefetch.h"


namespace nukes::dynamic {


/**
 * @details atomic mpsc queue base class, can be used as dyn spsc queue because they have same realisation
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct mpsc_queue {

    typedef details::nodes::dyn_node<dataT> node_t;      ///< Node type declaration
    typedef spmc_freelist<node_t> mempool_t;    ///< Memory buffer type

    class dyn_mpsc_iter {

        mpsc_queue* _queue { nullptr };

    public:
        explicit dyn_mpsc_iter(mpsc_queue* queue)
            : _queue(queue) {}

        dyn_mpsc_iter& prefix_increment(node_t*& ptr) {
            node_t* new_ptr = ptr->next();
            if (node_t* new_new_ptr = new_ptr->next(); _queue->recycle_dummy(new_ptr))
                new_ptr = new_new_ptr;
            _queue->_mempool.sync(ptr);
            ptr = new_ptr;
            return *this;
        }

        dyn_mpsc_iter postfix_increment(node_t*& ptr)  {
            dyn_mpsc_iter tmp = *this;
            node_t* new_ptr = ptr->next();
            if (node_t* new_new_ptr = new_ptr->next(); _queue->recycle_dummy(new_ptr))
                new_ptr = new_new_ptr;
            _queue->_mempool.sync(ptr);
            ptr = new_ptr;
            return tmp;
        }
    };

    typedef details::batch<node_t, dyn_mpsc_iter, mpsc_queue> batch_t;

protected:

    alignas(64) std::atomic<node_t*> _tail     ;  ///< Tail pointer
    node_t*                          _head     ;  ///< Head pointer
    node_t*                          _dummy_ptr;  ///< Dummy helper node

    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param node Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(details::misc::argument_ref_t<node_t*>) noexcept;

public:

    mempool_t          _mempool   {};  ///< Memory buffer to allocate nodes from

    mpsc_queue() noexcept : _dummy_ptr(nullptr) {
        if (not _mempool.capture(_dummy_ptr)) return;
        _head = _dummy_ptr;
        _tail.store(_dummy_ptr, std::memory_order_relaxed);
    };

    ~mpsc_queue() noexcept {
        while (_head != nullptr) {
            auto temp = std::exchange(_head,
                reinterpret_cast<node_t*>(_head->_next.load()));
            _mempool.sync(temp);
            if (_tail.load() == temp) {
                _head = nullptr;
                _tail.store(nullptr);
            }
        }
    }

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool push(dataT&& data) noexcept;

    /**
     * @details Atomically pushes element to the head of the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool push_front(dataT&& data) noexcept;

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
     * @details Atomically pushes node instance to the head of the queue (Move Semantics)
     * @param node Node instance to be pushed
     */
    void push_node_front(details::misc::argument_ref_t<node_t*> node) noexcept;

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
     * @details Извлекает всю очередь атомарно, в формате листа
     * @return Объект @b batch содержащий только операции получения итератора,
     * для прохода по списку
     */
    batch_t pop_batch() noexcept {
        node_t *current_head = _head, *current_tail = _tail.load(std::memory_order_relaxed);
        _head = current_tail;
        return batch_t { current_head, current_tail, this };
    }

    /**
     * @details Operation to get current head for inspection
     * @return @c const ptr to the head
     */
    [[nodiscard]] const node_t* inspect_head() noexcept;

    /**
     * @details Weak operation, can show that empty queue is not empty,
     * but it will never show that not empty queue is empty
     * @return @b True when queue is empty (guaranteed), @b False when queue might have elements
     */
    [[nodiscard]] bool empty() const noexcept;

    mpsc_queue(const mpsc_queue&) = delete;

    mpsc_queue(mpsc_queue&& q) noexcept : _dummy_ptr(nullptr) {
        this->_head = q._head;
        this->_tail = q._tail.load(std::memory_order_relaxed);
        this->_mempool = std::forward<mempool_t>(q._mempool);
        this->_dummy_ptr = q._dummy_ptr;
    }

    mpsc_queue operator=(mpsc_queue&) = delete;

    mpsc_queue& operator=(mpsc_queue&& q)  noexcept {
        this->_head = q._head;
        this->_tail = q._tail.load(std::memory_order_relaxed);
        this->_mempool = std::forward<mempool_t>(q._mempool);
        this->_dummy_ptr = q._dummy_ptr;
        return *this;
    }
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
push_front(dataT&& data) noexcept {

    node_t* new_node { nullptr };
    if (not _mempool.capture(new_node)) return false;
    new_node->_data = std::forward<dataT>(data);

    push_node_front(new_node);

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

    if (dummy == _dummy_ptr) [[unlikely]] {
        dummy->_next.store(nullptr, std::memory_order_release);
        node_t *current_tail = _tail.exchange(dummy, std::memory_order_release);
        current_tail->_next.store(dummy,std::memory_order_release);
        return true;
    }
    return false;
}


DYNAMIC_MPSC_QUEUE_MEMBER(void)
push_node(details::misc::argument_ref_t<node_t*> node) noexcept {

    node->_next.store(nullptr, std::memory_order_release);
    node_t* current_tail = _tail.exchange(node, std::memory_order_release);
    current_tail->_next.store(std::forward<node_t*>(node), std::memory_order_release);
}


DYNAMIC_MPSC_QUEUE_MEMBER(void)
push_node_front(details::misc::argument_ref_t<node_t*> node) noexcept {
    node->_next.store(_head, std::memory_order_release);
    _head = node;
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

    details::prefetch(_head);
    return std::forward<node_t*>(released_node);
}


DYNAMIC_MPSC_QUEUE_MEMBER(const nukes::dynamic::mpsc_queue<dataT>::node_t*)
inspect_head() noexcept {
    auto next_head = _head->_next.load(std::memory_order_relaxed);

    if (empty()) return nullptr;

    if (recycle_dummy(_head)) [[unlikely]] {
        _head = static_cast<node_t*>(next_head);
        return inspect_head();
    }

    return _head;
}


DYNAMIC_MPSC_QUEUE_MEMBER(bool)
empty() const noexcept {

    if (_head == _tail.load(std::memory_order_acquire))
        return true;
    else return false;
}



#undef DYNAMIC_MPSC_QUEUE_MEMBER
#endif // NUKES_DYNAMIC_MPSC_QUEUE
