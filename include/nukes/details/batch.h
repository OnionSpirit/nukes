#ifndef NUKES_DETAILS_BATCH
#define NUKES_DETAILS_BATCH

#include <concepts>


namespace nukes::details {

    template <typename nodeT, typename iteratorT>
    concept is_batch_iterable = requires(nodeT*& node, iteratorT iter) {
        { iter.prefix_increment(node) } -> std::same_as<iteratorT&>;
        { iter.postfix_increment(node) } -> std::same_as<iteratorT>;
    };

    template<typename nodeT, class iteratorImpl, typename ... iteratorImplArgs>
    class batch_iterator_traits {

        nodeT* _ptr;
        iteratorImpl _iter;

        static_assert(is_batch_iterable<nodeT, iteratorImpl>,
            "Iterator implementation is not actually an iterator");

        static_assert(std::constructible_from<iteratorImpl, iteratorImplArgs...>,
            "Passed iterator can't be constructed from passed args");

    public:

        explicit batch_iterator_traits(nodeT* ptr, iteratorImplArgs&& ... args)
            : _ptr(ptr)
            , _iter(std::forward<iteratorImplArgs>(args)...) {}

        auto& operator*() const { return _ptr->_data; }
        nodeT* operator->() { return _ptr; }

        batch_iterator_traits& operator++() {
            _iter = _iter.prefix_increment(_ptr);
            return *this;
        }

        batch_iterator_traits operator++(int) {
            _iter = _iter.postfix_increment(_ptr);
            return *this;
        }

        bool operator==(const batch_iterator_traits& other) const { return _ptr == other._ptr; }
        bool operator!=(const batch_iterator_traits& other) const { return _ptr != other._ptr; }
    };

    template <typename nodeT, class iteratorT, typename ... iteratorArgs>
    class batch {

        nodeT* _head;
        nodeT* _tail;
        std::tuple<iteratorArgs...> _args{};

    public:

        explicit batch() =default;

        explicit batch(nodeT* head, nodeT* tail, iteratorArgs&& ... args)
            : _head(head)
            , _tail(tail)
            , _args(args...) {}

        typedef batch_iterator_traits<nodeT, iteratorT, iteratorArgs...> iterator_t;

        iterator_t begin() { return iterator_t(_head, std::forward<iteratorArgs>(std::get<iteratorArgs>(_args))...); }
        iterator_t end() { return iterator_t(_tail, std::forward<iteratorArgs>(std::get<iteratorArgs>(_args))...); }

        iterator_t begin() const { return iterator_t(_head, std::forward<iteratorArgs>(std::get<iteratorArgs>(_args))...); }
        iterator_t end() const { return iterator_t(_tail, std::forward<iteratorArgs>(std::get<iteratorArgs>(_args))...); }

        nodeT* get_head() { return _head; }
        nodeT* get_tail() { return _tail; }
    };

    // NOTE: Pushing prev node ptr to dummy data section
    template <typename node_t>
    void set_prev_to_dummy(node_t*& prev, node_t*& dummy) {
        void* mem_ptr = &dummy->_data;
        auto** data_ptr = static_cast<node_t**>(mem_ptr);
        *data_ptr = prev;
    }

    // NOTE: Taking dummy back from batch
    template <typename queue_t>
    void eject_dummy(queue_t* dummy_owner) {
        if (auto* before_dummy = reinterpret_cast<queue_t::node_t*>(dummy_owner->_dummy_ptr->_data)) [[likely]]
           before_dummy->_next.store(dummy_owner->_dummy_ptr->_next.load(std::memory_order_relaxed), std::memory_order_relaxed);
        if (not dummy_owner->recycle_dummy(dummy_owner->_dummy_ptr))
            std::cerr << "dummy recycling rejected\n";
    }

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
