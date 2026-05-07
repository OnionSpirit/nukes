/**
 * @file
 * @details Contains freelist declaration
 */
#ifndef NUKES_REGULAR_FREELIST
#define NUKES_REGULAR_FREELIST


#include <atomic>
#include <cstddef>
#include <cstdint>

#include "constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "nukes/details/prefetch.h"


namespace nukes::dynamic {


/**
 * @details freelist class
 * @tparam dataT Type that assumed to be used in the freelist
 * @tparam _ Placeholder to iface compatibility
 */
template <typename dataT, size_t _ = 0>
struct reg_freelist {

protected:

    typedef details::nodes::dyn_reg_node<dataT> node_t;  ///< Node type declaration

    node_t* _head { }; ///< Head pointer
    node_t* _tail { }; ///< Tail pointer

public:

    explicit reg_freelist() noexcept = default;

    ~reg_freelist() noexcept;

    /**
     * @details Atomically pushes element to the queue
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool sync(dataT*& data) noexcept;

    /**
     * @details Atomically pushes element to the queue without destruction
     * @param data Data to be pushed
     * @return @b True if element successfully pushed,
     * @b False when run out of capacity
     */
    bool raw_sync(void* data) noexcept;

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

    reg_freelist operator=(reg_freelist&) = delete;

    reg_freelist& operator=(reg_freelist&& q)  noexcept {
        this->_head = q._head;
        this->_tail = q._tail;
        q._head = nullptr;
        q._tail = nullptr;
        return *this;
    }
};

} // end namespace nukes::memory


// ================================ DEFINITIONS ================================

#define REGULAR_FREELIST_MEMBER(member_type)                     \
    template <typename dataT, size_t _ >                         \
        member_type nukes::dynamic::reg_freelist <dataT, _>::

REGULAR_FREELIST_MEMBER()
~reg_freelist() noexcept {

    while (_head != nullptr) {
        auto temp = _head;
        _head = _head->_next;

        free(temp);
        if (_tail == temp) {
            _head = nullptr;
            _tail = nullptr;
        }
    }
}


REGULAR_FREELIST_MEMBER(bool)
sync(dataT*& data) noexcept {
    data->~dataT();
    auto* new_tail = reinterpret_cast<node_t *>(reinterpret_cast<uint8_t *>(data) -
        [] { node_t t{}; return reinterpret_cast<uintptr_t>(&t._data) - reinterpret_cast<uintptr_t>(&t); }());
    new_tail->_next = nullptr;

    // NOTE: Setting tail depending on it's state
    if (_tail == nullptr) [[unlikely]] {
        _head = new_tail;
        _tail = new_tail;
    }
    // NOTE: Standard single producing push back
    else [[likely]] {
        _tail->_next = new_tail;
        _tail = new_tail;
    }

    data = nullptr;
    return true;
}

REGULAR_FREELIST_MEMBER(bool)
raw_sync(void* data) noexcept {
    auto* new_tail = reinterpret_cast<node_t *>(static_cast<uint8_t *>(data) -
        [] { node_t t{}; return reinterpret_cast<uintptr_t>(&t._data) - reinterpret_cast<uintptr_t>(&t); }());
    new_tail->_next = nullptr;

    // NOTE: Setting tail depending on it's state
    if (_tail == nullptr) [[unlikely]] {
        _head = new_tail;
        _tail = new_tail;
    }
    // NOTE: Standard single producing push back
    else [[likely]] {
        _tail->_next = new_tail;
        _tail = new_tail;
    }

    return true;
}


REGULAR_FREELIST_MEMBER(bool)
capture(dataT*& data) noexcept {
    if (not _head) [[unlikely]] {
        _tail = nullptr;
        data = std::forward<dataT*>(&(new node_t)->_data);
        return data not_eq nullptr;
    }
    data = std::forward<dataT*>(&_head->_data);
    _head = _head->_next;
    details::prefetch(_head);
    return true;
}

REGULAR_FREELIST_MEMBER(bool)
empty() noexcept { return _head == nullptr; }


#undef REGULAR_FREELIST_MEMBER
#endif // NUKES_REGULAR_FREELIST
