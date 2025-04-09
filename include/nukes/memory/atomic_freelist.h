/**
 * @file
 * @details Contains atomic_freelist declaration
 */
#ifndef NUKES_MEMORY_ATOMIC_FREELIST
#define NUKES_MEMORY_ATOMIC_FREELIST


#include <atomic>
#include <cstddef>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"


namespace nukes::memory {


/**
 * @details freelist class
 * @tparam dataT Type that assumed to be used in the freelist
 * @tparam _ Placeholder to iface compatibility
 */
template <typename dataT, size_t _>
struct atomic_freelist {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;  ///< Node type declaration
    node_t    _dummy {};    ///< Dummy node instance

    alignas(64) std::atomic<node_t*> _head { &_dummy }; ///< Head pointer
    alignas(64) std::atomic<node_t*> _tail { &_dummy }; ///< Tail pointer

    /**
     * @details Recycles node, if it's dummy, to the end of the freelist
     * @param node Node instance
     * @return @b True if node was dummy and recycled to freelist tail, @b False otherwise
     */
    [[nodiscard]] bool recycle_dummy(node_t*& n) noexcept;

public:

    atomic_freelist() noexcept = default;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    void sync(dataT*& data) noexcept;

    /**
     * @details Atomically pops an element from the queue to the reference
     * from function arg, returns the result of operation
     * @param data Reference to storage of pulled element
     * @return @b True if element successfully pulled,
     * @b False when pulling failed or data node sync failed
     */
    [[nodiscard]] bool capture(dataT*& data) noexcept;

    /**
     * @details Weak operation, can show that empty freelist is not empty,
     * but it will never show that not empty freelist is empty
     * @return @b True when freelist is empty (guaranteed), @b False when freelist might have elements
     */
    [[nodiscard]] bool empty() noexcept;
};

} // end namespace nukes::memory


// ================================ DEFINITIONS ================================

#define ATOMIC_FREELIST_MEMBER(member_type)                     \
    template <typename dataT, size_t _ >                        \
        member_type nukes::memory::atomic_freelist <dataT, _>::


ATOMIC_FREELIST_MEMBER(bool)
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


ATOMIC_FREELIST_MEMBER(void)
sync(dataT*& data) noexcept {

    node_t node = *(node_t*)((uint8_t*)data - offsetof(node_t, _data));
    node->_next.store(node_t{}, std::memory_order_release);

    node_t new_tail, current_tail = _tail.load(std::memory_order_acquire);
    new_tail = node;
    while (not _tail.compare_exchange_weak(current_tail, new_tail, std::memory_order_release,
                                           std::memory_order_relaxed)) {}
    reinterpret_cast<node_t*>(current_tail)
        ->_next.store(new_tail, std::memory_order_release);

    data = nullptr;
}


ATOMIC_FREELIST_MEMBER(bool)
capture(dataT*& data) noexcept {
    while (true) {
        node_t node, new_head, current_head = _head.load(std::memory_order_acquire);

        do {if (not current_head) [[unlikely]] return false;
            new_head = reinterpret_cast<node_t *>(current_head)->_next.load()._node;
            if (not new_head) {
                data = std::forward<dataT*>(&(new node_t)->_data);
                return true;
            }
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        node = std::forward<node_t*>(reinterpret_cast<node_t*>(current_head));

        if (not recycle_dummy(node)) [[likely]] {
            data = std::forward<dataT>(node->_data);
            return true;
        }
    }
}


ATOMIC_FREELIST_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else return false;
}


#undef ATOMIC_FREELIST_MEMBER
#endif // NUKES_MEMORY_ATOMIC_FREELIST
