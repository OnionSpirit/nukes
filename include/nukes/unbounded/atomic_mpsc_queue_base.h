/**
 * @file
 * @details Contains atomic_mpsc_queue_base declaration
 */
#ifndef RIOT_NUKES_ATOMIC_MPSC_QUEUE_BASE
#define RIOT_NUKES_ATOMIC_MPSC_QUEUE_BASE


#include <atomic>

#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"



namespace nukes {

    
/**
 * @details atomic mpsc queue base class
 * @tparam dataT Type that assumed to be used in the queue
 */
template <typename dataT>
struct atomic_mpsc_queue_base {
    
protected:

    typedef dyn_node<dataT> node;                ///< Node type declaration
    
    node*                     _head { &_dummy }; ///< Head pointer
    std::atomic<dyn_node_hdl> _tail {};          ///< Tail pointer

    node _dummy {};                              ///< Dummy node instance
    
    /**
     * @details Recycles node, if it's dummy, to the end of the queue
     * @param node Node instance
     * @return @b True if node was dummy and recycled to queue tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(node*& n) noexcept;

public:

    atomic_mpsc_queue_base() noexcept;
    
    virtual ~atomic_mpsc_queue_base() = default;

    /**
     * @details Atomically pushes element to the queue with allocation
     * @param data Data to be pushed
     */
    void push_new(fn_forward_t<dataT> data) noexcept;

    /**
     * @details Pops an element from the queue to the reference
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
     * @details Pops an node instance from the queue to
     * the reference from function arg and returns the result of operation
     * @param node Reference to pulled node storage
     * @return @b True if node instance successfully pulled,
     * @b False when pulling failed
     */
    [[nodiscard]] bool pop_node(node *&n) noexcept;

    /**
     * @details Shows that queue is empty
     * @return @b True when queue is empty, @b False when queue might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

 
} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_MPSC_QUEUE_BASE_MEMBER(member_type)                              \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpsc_queue_base<dataT>::


ATOMIC_MPSC_QUEUE_BASE_MEMBER()
atomic_mpsc_queue_base() noexcept {

    const dyn_node_hdl initial_hdl {
        ._node = &_dummy,
        ._tag  = 0
    };

    _tail.store(initial_hdl);
};


ATOMIC_MPSC_QUEUE_BASE_MEMBER(bool)
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


ATOMIC_MPSC_QUEUE_BASE_MEMBER(void)
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


ATOMIC_MPSC_QUEUE_BASE_MEMBER(bool)
pop_new(dataT &data) noexcept {

    do {
        const node* new_head = _head->_next.load();
        if (nullptr == new_head) [[unlikely]] return false;
        else [[likely]] {
            data = std::forward<dataT>(_head->_data);
            _head = new_head;
        }
    } while (recycle_dummy(_head));

    return true;
}


ATOMIC_MPSC_QUEUE_BASE_MEMBER(void)
push_node(fn_forward_t<node> n) noexcept {
    
    n->_next.store(dyn_node_hdl{}, std::memory_order_release);
    
    dyn_node_hdl new_tail_hdl, tail_hdl = _tail.load(std::memory_order_acquire);
    new_tail_hdl._node = n;       
    while (not _tail.compare_exchange_weak(tail_hdl, new_tail_hdl, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node*>(tail_hdl._node)
        ->_next.store(new_tail_hdl,std::memory_order_release);
}


ATOMIC_MPSC_QUEUE_BASE_MEMBER(bool)
pop_node(node*& n) noexcept {

    do {
        const node* new_head = _head->_next.load();
        if (nullptr == new_head) [[unlikely]] return false;
        else [[likely]] n = _head;
    } while (recycle_dummy(_head));

    return true;
}


ATOMIC_MPSC_QUEUE_BASE_MEMBER(bool)
empty() noexcept {

    if (_head == _tail.load(std::memory_order_acquire)._node)
        return true;
    else return false;
}


#undef ATOMIC_MPSC_QUEUE_BASE_MEMBER
#endif // NUKES_ATOMIC_MPSC_QUEUE_BASE
