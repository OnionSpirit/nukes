#ifndef NUKES_META
#define NUKES_META



#define ATOMIC_BOUNDED_FREELIST_MEMBER(member_type)                             \
    template <typename ChunkType, uint32_t ssize>                               \
    member_type nukes::atomic_bounded_freelist<ChunkType, ssize>::


#define ATOMIC_UNBOUNDED_FREELIST_MEMBER(member_type)                           \
    template <typename ChunkType, size_t BucketByteSize,                        \
            void *(mem_alloc)(size_t), void(mem_free)(void *)>                  \
    requires (constants::ufl_memory_offset                                      \
              + sizeof(meta_chunk<ChunkType>) <= BucketByteSize)                \
    member_type nukes::atomic_unbounded_freelist <ChunkType,                    \
                  BucketByteSize, mem_alloc, mem_free>::


#define ATOMIC_BOUNDED_STACK_MEMBER(member_type)                                \
    template <typename dataT, uint32_t ssize>                                   \
    member_type nukes::atomic_bounded_stack<dataT, ssize>::


#define ATOMIC_UNBOUNDED_STACK_MEMBER(member_type)                              \
    template                                                                    \
    <                                                                           \
        typename dataT,                                                         \
        size_t BucketByteSize,                                                  \
        void* (mem_alloc) (size_t),                                             \
        void  (mem_free)  (void*)                                               \
    >                                                                           \
    member_type nukes::atomic_unbounded_stack<dataT, BucketByteSize,            \
                                              mem_alloc, mem_free>::


#define ATOMIC_STACK_BASE_MEMBER(member_type)                                   \
    template <typename dataT>                                                   \
    member_type nukes::atomic_stack_base<dataT>::


#define ATOMIC_MPMC_QUEUE_BASE_MEMBER(member_type)                              \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpmc_queue_base<dataT>::


#define ATOMIC_MPSC_QUEUE_BASE_MEMBER(member_type)                              \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpsc_queue_base<dataT>::


#define ATOMIC_MPMC_BOUNDED_QUEUE_MEMBER(member_type)                           \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpmc_bounded_queue<dataT>::


#define ATOMIC_MPSC_BOUNDED_QUEUE_MEMBER(member_type)                           \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpsc_bounded_queue<dataT>::


#define ATOMIC_MPMC_UNBOUNDED_QUEUE_MEMBER(member_type)                         \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpmc_unbounded_queue<dataT>::


#define ATOMIC_MPSC_UNBOUNDED_QUEUE_MEMBER(member_type)                         \
    template<typename dataT>                                                    \
    member_type nukes::atomic_mpsc_unbounded_queue<dataT>::



#endif // NUKES_META

