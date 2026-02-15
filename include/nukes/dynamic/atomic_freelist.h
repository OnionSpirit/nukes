/**
 * @file
 * @details Contains atomic_freelist declaration
 */
#ifndef NUKES_DYNAMIC_ATOMIC_FREELIST
#define NUKES_DYNAMIC_ATOMIC_FREELIST


#include <atomic>
#include <cstddef>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"


namespace nukes::dynamic {


/**
 * @details freelist class
 * @tparam dataT Type that assumed to be used in the freelist
 * @tparam _ Placeholder to iface compatibility
 */
template <typename dataT, size_t _ = 0>
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

    explicit atomic_freelist() noexcept = default;

    ~atomic_freelist() noexcept;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool sync(dataT*& data) noexcept;

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

    atomic_freelist operator=(atomic_freelist&) = delete;

    atomic_freelist& operator=(atomic_freelist&& q)  noexcept {
        this->_head = q._head.load(std::memory_order_relaxed);
        this->_tail = q._tail.load(std::memory_order_relaxed);
        return *this;
    }
};

} // end namespace nukes::memory


// ================================ DEFINITIONS ================================

#define DYNAMIC_ATOMIC_FREELIST_MEMBER(member_type)                     \
    template <typename dataT, size_t _ >                        \
        member_type nukes::dynamic::atomic_freelist <dataT, _>::

DYNAMIC_ATOMIC_FREELIST_MEMBER()
~atomic_freelist() noexcept {

    while (_head.load() != nullptr) {
        auto temp = _head.load();
        _head.store(reinterpret_cast<node_t*>(_head.load()->_next.load()));
        if (reinterpret_cast<ulong>(temp) == reinterpret_cast<ulong>(&_dummy))
            continue;

        free(temp);
        if (_tail.load() == temp) {
            _head.store(nullptr);
            _tail.store(nullptr);
        }
    }
}

DYNAMIC_ATOMIC_FREELIST_MEMBER(bool)
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


DYNAMIC_ATOMIC_FREELIST_MEMBER(bool)
sync(dataT*& data) noexcept {
    data->~dataT();
    auto* new_tail = reinterpret_cast<node_t *>(reinterpret_cast<uint8_t *>(data) -
        [] { node_t t{}; return reinterpret_cast<ulong>(&t._data) - reinterpret_cast<ulong>(&t); }());
    new_tail->_next.store(nullptr, std::memory_order_relaxed);

    node_t *current_tail = _tail.load(std::memory_order_relaxed);
    while (not _tail.compare_exchange_weak(current_tail, new_tail
                                           , std::memory_order_release
                                           , std::memory_order_relaxed)) {}
    current_tail->_next.store(new_tail,std::memory_order_release);

    data = nullptr;
    return true;
}


DYNAMIC_ATOMIC_FREELIST_MEMBER(bool)
capture(dataT*& data) noexcept {
    while (true) {
        node_t *new_head, *current_head = _head.load(std::memory_order_acquire);

        do {
            // // NOTE: Делаем через goto, потому что continue станет проверять условие
            // //       с CAS операцией, а нам такого не надо
            // loop_begin:
            if (not current_head) [[unlikely]] {
                data = std::forward<dataT*>(&(new node_t)->_data);
                return true;
            }
            new_head = reinterpret_cast<node_t *>(current_head->_next.load());
            if (not new_head) [[unlikely]] {
                data = std::forward<dataT*>(&(new node_t)->_data);
                return true;
            }
            // if (_head.load(std::memory_order_relaxed) != current_head) [[unlikely]] {
            //     current_head = _head.load(std::memory_order_relaxed);
            //     goto loop_begin;
            // }
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        auto* node = std::forward<node_t*>(current_head);
        if (not recycle_dummy(node)) [[likely]] {
            data = std::forward<dataT*>(&node->_data);
            return true;
        }
    }
}

DYNAMIC_ATOMIC_FREELIST_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    else return false;
}


#undef DYNAMIC_ATOMIC_FREELIST_MEMBER
#endif // NUKES_DYNAM_ATOMIC_FREELIST
