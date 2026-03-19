/**
 * @file
 * @details Contains spmc_freelist declaration
 */
#ifndef NUKES_DYNAMIC_SPMC_FREELIST
#define NUKES_DYNAMIC_SPMC_FREELIST


#include <atomic>
#include <cstddef>
#include <cstdint>

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
struct spmc_freelist {

protected:

    typedef details::nodes::dyn_node<dataT> node_t;  ///< Node type declaration

    alignas(64) std::atomic<node_t*> _head      { }; ///< Head pointer
    alignas(64) node_t*              _tail      { }; ///< Tail pointer

public:

    explicit spmc_freelist() noexcept = default;

    ~spmc_freelist() noexcept;

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

    spmc_freelist operator=(spmc_freelist&) = delete;

    spmc_freelist& operator=(spmc_freelist&& q)  noexcept {
        this->_head.store(q._head.load(std::memory_order_relaxed), std::memory_order_relaxed);
        this->_tail = q._tail;
        return *this;
    }
};

} // end namespace nukes::memory


// ================================ DEFINITIONS ================================

#define DYNAMIC_SPMC_FREELIST_MEMBER(member_type)                     \
    template <typename dataT, size_t _ >                        \
        member_type nukes::dynamic::spmc_freelist <dataT, _>::

DYNAMIC_SPMC_FREELIST_MEMBER()
~spmc_freelist() noexcept {

    while (_head.load() != nullptr) {
        auto temp = _head.load();
        _head.store(reinterpret_cast<node_t*>(_head.load()->_next.load()));

        free(temp);
        if (_tail == temp) {
            _head.store(nullptr);
            _tail = nullptr;
        }
    }
}


DYNAMIC_SPMC_FREELIST_MEMBER(bool)
sync(dataT*& data) noexcept {
    data->~dataT();
    auto* new_tail = reinterpret_cast<node_t *>(reinterpret_cast<uint8_t *>(data) -
        [] { node_t t{}; return reinterpret_cast<uintptr_t>(&t._data) - reinterpret_cast<uintptr_t>(&t); }());
    new_tail->_next.store(nullptr, std::memory_order_relaxed);

    // NOTE: Setting tail depending on it's state
    if (_tail == nullptr) [[unlikely]]
        _tail = new_tail;
    // NOTE: Standard single producing push back
    else [[likely]] {
        _tail->_next.store(new_tail,std::memory_order_release);
        _tail = new_tail;
    }
    // NOTE: Setting head if it is null
    if (_head.load(std::memory_order_relaxed) == nullptr) [[unlikely]]
        _head.store(new_tail, std::memory_order_release);

    data = nullptr;
    return true;
}


DYNAMIC_SPMC_FREELIST_MEMBER(bool)
capture(dataT*& data) noexcept {
    while (true) {
        node_t *new_head, *current_head = _head.load(std::memory_order_acquire);

        do {
            cxhg_loop:
            if (not current_head) [[unlikely]] {
                data = std::forward<dataT*>(&(new node_t)->_data);
                return data not_eq nullptr;
            }
            new_head = reinterpret_cast<node_t *>(current_head->_next.load());
            if (not new_head) [[unlikely]] {
                data = std::forward<dataT*>(&(new node_t)->_data);
                return data not_eq nullptr;
            }
            if (node_t* actual_head = _head.load(std::memory_order_acquire);
                actual_head not_eq current_head) [[unlikely]] {
                current_head = actual_head;
                goto cxhg_loop;
            }
        } while (not _head.compare_exchange_weak(current_head, new_head, std::memory_order_release,
                                                 std::memory_order_relaxed));

        auto* node = std::forward<node_t*>(current_head);
        data = std::forward<dataT*>(&node->_data);
        return true;
    }
}

DYNAMIC_SPMC_FREELIST_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    if (_tail == head)
        return true;
    return false;
}


#undef DYNAMIC_SPMC_FREELIST_MEMBER
#endif // NUKES_DYNAMIC_SPMC_FREELIST
