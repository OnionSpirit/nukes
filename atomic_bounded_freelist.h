#ifndef NUKES_ATOMIC_BOUNDED_FREELIST
#define NUKES_ATOMIC_BOUNDED_FREELIST

#include <atomic>
#include <cstdint>

#include "helpers.h"
#include "node_types.h"
#include "meta.h"



namespace nukes {


template <typename ChunkType, uint32_t ssize = 1024>
struct atomic_bounded_freelist {

protected:
    
    typedef mem_node<ChunkType> chunk_node_t;
    typedef std::atomic<stc_node_hdl> node_hdl_t;
    
    node_hdl_t   _top           {};
    chunk_node_t _buffer[ssize] {};

public:
    
    atomic_bounded_freelist() noexcept;
    
    ~atomic_bounded_freelist() noexcept =default;
    
    [[nodiscard]] ChunkType* ptr_by_idx(uint32_t idx) noexcept;

    [[nodiscard]] uint32_t idx_by_ptr(ChunkType* ptr) const noexcept;
    
    [[nodiscard]] bool sync_idx(uint32_t& idx) noexcept;
    
    [[nodiscard]] bool capture_idx(uint32_t& idx) noexcept;
    
    [[nodiscard]] bool sync(ChunkType*& ptr) noexcept;
    
    [[nodiscard]] bool capture(ChunkType*& ptr) noexcept;
};


} // end namespace nukes

 
// ================================ DEFINITIONS ================================


ATOMIC_BOUNDED_FREELIST_MEMBER()
atomic_bounded_freelist() noexcept {
    stc_node_hdl next {._node_idx = 0, ._tag = 0};
    _top.store(next);
    for (int i =0; i < ssize - 1; ++i) {
        next._node_idx = (uint32_t)(i + 1);
        _buffer[i]._next.store(next);
    }
}


ATOMIC_BOUNDED_FREELIST_MEMBER(ChunkType*)
ptr_by_idx(uint32_t idx) noexcept {
    return idx < ssize ? &(_buffer[idx]._mem) : nullptr;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(uint32_t)
idx_by_ptr(ChunkType* ptr) const noexcept {
    
    const uint64_t normalized_addr
        { ((uint64_t)ptr - (uint64_t)&_buffer[0] - 8) };
    const uint32_t idx
        { static_cast<uint32_t>(normalized_addr / sizeof(chunk_node_t)) };
    return idx < ssize ? idx : UINT32_MAX;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
sync_idx(uint32_t &idx) noexcept {

    if (idx >= ssize) [[unlikely]] return false;

    chunk_node_t* new_node = &_buffer[idx];
    stc_node_hdl new_top_hdl, top_hdl = _top.load();
    new_top_hdl._node_idx = idx;        
    
    do {
        new_top_hdl._tag = top_hdl._tag + 1;
        new_node->_next.store(top_hdl);
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    idx = UINT32_MAX;
    return true;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
capture_idx(uint32_t& idx) noexcept {

    stc_node_hdl new_top_hdl, top_hdl = _top.load();
    
    do { if (top_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;

        new_top_hdl._tag
            = top_hdl._tag + 1;
        new_top_hdl._node_idx
            = _buffer[top_hdl._node_idx]._next.load()._node_idx;
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    idx = top_hdl._node_idx;
    return true;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
sync(ChunkType*& ptr) noexcept {

    const uint64_t idx = idx_by_ptr(ptr);
    if (idx >= ssize) [[unlikely]] return false;

    chunk_node_t* new_node = &_buffer[idx];
    stc_node_hdl new_top_hdl, top_hdl = _top.load();
    new_top_hdl._node_idx = idx;
    
    do {
        new_top_hdl._tag = top_hdl._tag + 1;
        new_node->_next.store(top_hdl);
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    ptr = nullptr;
    return true;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
capture(ChunkType*& ptr) noexcept {   

    stc_node_hdl new_top_hdl, top_hdl = _top.load();
    
    do { if (top_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;
        
        new_top_hdl._tag
            = top_hdl._tag + 1;
        new_top_hdl._node_idx
            = _buffer[top_hdl._node_idx]._next.load()._node_idx;
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    ptr = &(_buffer[top_hdl._node_idx]._mem);
    return true;
}


#undef ATOMIC_BOUNDED_FREELIST_MEMBER
#endif // NUKES_ATOMIC_BOUNDED_FREELIST
