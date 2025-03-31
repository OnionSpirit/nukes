#ifndef NUKES_META
#define NUKES_META


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

