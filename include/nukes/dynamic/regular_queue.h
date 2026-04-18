/**
 * @file
 * @details Contains queue_base declaration
 */
#ifndef NUKES_REGULAR_QUEUE
#define NUKES_REGULAR_QUEUE


#include <atomic>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "regular_freelist.h"


namespace nukes::dynamic {


/**
 * @details atomic mpsc queue base class, can be used as dyn spsc queue because they have same realisation
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct reg_queue {

    typedef details::nodes::dyn_reg_node<dataT> node_t;      ///< Node type declaration
    typedef reg_freelist<node_t> mempool_t;    ///< Memory buffer type

protected:

    node_t* _head;  ///< Head pointer
    node_t* _tail;  ///< Tail pointer

    mempool_t          _mempool   {};  ///< Memory buffer to allocate nodes from

public:

    reg_queue() = default;

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

    reg_queue(const reg_queue&) = delete;

    reg_queue(reg_queue&& q) noexcept {
        this->_head = q._head;
        this->_tail = q._tail;
        q._head = nullptr;
        q._tail = nullptr;
    }

    reg_queue operator=(reg_queue&) = delete;

    reg_queue& operator=(reg_queue&& q)  noexcept {
        this->_head = q._head;
        this->_tail = q._tail;
        q._head = nullptr;
        q._tail = nullptr;
        return *this;
    }
};

} // end namespace nukes::dynamic


// ================================ DEFINITIONS ================================


#define REGULAR_QUEUE_MEMBER(member_type)         \
    template <typename dataT>                          \
        member_type nukes::dynamic::reg_queue<dataT>::


REGULAR_QUEUE_MEMBER(bool)
push(dataT&& data) noexcept {

    node_t* new_node { nullptr };
    if (not _mempool.capture(new_node)) return false;
    new_node->_data = std::forward<dataT>(data);

    push_node(new_node);

    return true;
}


REGULAR_QUEUE_MEMBER(bool)
pop(dataT& data) noexcept {

    auto* released_node = pop_node();
    if (not released_node) [[unlikely]] return false;

    data = std::forward<dataT>(released_node->_data);
    return _mempool.sync(released_node);
}


REGULAR_QUEUE_MEMBER(void)
release_node(details::misc::argument_ref_t<node_t*> node) noexcept {
    _mempool.sync(node);
}


REGULAR_QUEUE_MEMBER(void)
push_node(details::misc::argument_ref_t<node_t*> node) noexcept {
    node->_next = nullptr;
    if (_tail == nullptr) {
        _head = node;
        _tail = node;
    } else {
        _tail->_next = node;
        _tail = node;
    }
}


REGULAR_QUEUE_MEMBER(auto)
pop_node() noexcept -> node_t* {
    if (not _head) {
        _tail = nullptr;
        return _head;
    }
    auto released_node = _head;
    _head = _head->_next;
    return std::forward<node_t*>(released_node);
}

REGULAR_QUEUE_MEMBER(bool)
empty() noexcept { return _head == nullptr; }



#undef REGULAR_QUEUE_MEMBER
#endif // NUKES_REGULAR_QUEUE
