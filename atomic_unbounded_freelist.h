#ifndef NUKES_ATOMIC_UNBOUNDED_FREELIST
#define NUKES_ATOMIC_UNBOUNDED_FREELIST

#include <atomic>

#include "constants.h"
#include "helpers.h"
#include "meta.h"
#include "node_types.h"
#include "atomic_bounded_freelist.h"



namespace nukes {


template
<
    typename ChunkType,
    
    size_t BucketByteSize =
        constants::ufl_memory_offset
      + sizeof(meta_chunk<ChunkType>) * 64,

    void* (mem_alloc) (size_t) = malloc,
    
    void  (mem_free)  (void*)  = free
>
requires (constants::ufl_memory_offset + sizeof(meta_chunk<ChunkType>) <= BucketByteSize)
struct atomic_unbounded_freelist {

protected:
    
    typedef meta_chunk<ChunkType> metaChunk;

    static constexpr size_t meta_chunk_count =
        (BucketByteSize - constants::ufl_memory_offset) / sizeof(metaChunk); ///< Wanted to fit whole bucket_node
                                                                             ///< with bucket into BucketBytesCount

    typedef atomic_bounded_freelist<metaChunk, meta_chunk_count> bucket;
    typedef bucket* bucket_ptr;

    struct alignas(8) bucket_node {
        std::atomic<bucket_node*> _next       { nullptr };
        bucket_ptr                _bucket_ptr { nullptr };
    };

    bucket _basic_bucket {};

    std::atomic_flag _allocation_in_progress = ATOMIC_FLAG_INIT;

    bucket_node _basic_bucket_node {
        ._next       = &_basic_bucket_node,
        ._bucket_ptr = &_basic_bucket
    };

    std::atomic<bucket_node*> _current_bucket_node { &_basic_bucket_node };

    std::atomic<bucket_node*> _last_bucket_node    { &_basic_bucket_node };
    
    static void allocate_bucket_node(bucket_node*& node);

    static void deallocate_bucket_node(bucket_node*& node);

public:
    
    atomic_unbounded_freelist(size_t buckets_count = 1);

    ~atomic_unbounded_freelist();
    
    [[nodiscard]] bool sync(ChunkType*& ptr) noexcept;
        
    [[nodiscard]] bool capture(ChunkType*& ptr) noexcept;
};


// ================================ DEFINITIONS ================================


ATOMIC_UNBOUNDED_FREELIST_MEMBER()    
atomic_unbounded_freelist(size_t buckets_count) {
    
    bucket_node* current = &_basic_bucket_node;
    for (int i = 1; i < buckets_count; ++i) {
        allocate_bucket_node(current);
        current = current->_next;
    }
}


ATOMIC_UNBOUNDED_FREELIST_MEMBER()
~atomic_unbounded_freelist() {
    
    bucket_node* current = _basic_bucket_node._next.load();

    int buckets_count {0};
    while (current) { ++buckets_count; current = current->_next.load(); }

    current = _basic_bucket_node._next.load();
    for (int i = 1; i < buckets_count and current; ++i) {
        auto next = current->_next.load();
        delete current;
        current = next;
    }
}


ATOMIC_UNBOUNDED_FREELIST_MEMBER(void)
allocate_bucket_node(bucket_node*& node) {
    
    uint8_t* bucket { (uint8_t*)mem_alloc(BucketByteSize) };
    node = reinterpret_cast<bucket_node*>(bucket);
    node->_bucket_ptr = reinterpret_cast<bucket_ptr>(bucket + 8);
}


ATOMIC_UNBOUNDED_FREELIST_MEMBER(void)
deallocate_bucket_node(bucket_node*& node) {
    
    uint8_t bucket [BucketByteSize] = reinterpret_cast<uint8_t*>(node);
    mem_free(bucket);
    node = nullptr;
}


ATOMIC_UNBOUNDED_FREELIST_MEMBER(bool)
sync(ChunkType*& ptr) noexcept {

    metaChunk* mem = (metaChunk*)((uint8_t*)ptr - sizeof(meta_data<>));

    bucket_ptr sync_bucket { nullptr };
    mem->_meta_data.release(sync_bucket);
    const bool res = sync_bucket->sync(mem);
    
    ptr = nullptr;
    return res;
}


ATOMIC_UNBOUNDED_FREELIST_MEMBER(bool)
capture(ChunkType*& ptr) noexcept {
    
    bucket_node* selected_bucket = _current_bucket_node.load();
    bucket_node* initial_bucket  = selected_bucket;
    metaChunk* mem;

    while (selected_bucket->_bucket_ptr and not selected_bucket->_bucket_ptr->capture(mem)) {
        selected_bucket = selected_bucket->_next.load();

        if (not selected_bucket) selected_bucket = &_basic_bucket_node;

        if (selected_bucket == initial_bucket
            and not _allocation_in_progress.test_and_set()) {
                
            bucket_node* new_bucket { nullptr };
            allocate_bucket_node(new_bucket);
            _last_bucket_node.load()->_next.store(new_bucket);
            
            _last_bucket_node.store(new_bucket);
            _current_bucket_node.store(new_bucket);
            selected_bucket = new_bucket;
            
            _allocation_in_progress.clear();
        }
    }

    mem->_meta_data = selected_bucket->_bucket_ptr;
    ptr = &(mem->_mem);
    _current_bucket_node.store(selected_bucket);
    
    return true;
}


} // end namespace nukes


#undef ATOMIC_UNBOUNDED_FREELIST_MEMBER
#endif // NUKES_ATOMIC_UNBOUNDED_FREELIST
