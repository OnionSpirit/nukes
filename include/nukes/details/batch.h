#ifndef NUKES_DETAILS_BATCH
#define NUKES_DETAILS_BATCH

#include <concepts>


namespace nukes::details {

    template <typename nodeT, typename iteratorT>
    concept is_batch_iterable = requires(nodeT*& node, iteratorT iter) {
        { iter.postfix_increment(node) } -> std::same_as<iteratorT&>;
        { iter.prefix_increment(node) } -> std::same_as<iteratorT>;
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
            _iter = _iter.postfix_increment(_ptr);
            return *this;
        }

        batch_iterator_traits operator++(int) {
            _iter = _iter.prefix_increment(_ptr);
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

        //     iterator& operator++() {
        //         nodeT* new_ptr = _ptr->next();
        //         if (_dummy_ptr and new_ptr == _dummy_ptr) {
        //             new_ptr = new_ptr->next();
        //         }
        //         _mempool->sync(_ptr);
        //         _ptr = new_ptr;
        //         return *this;
        //     }

        //     iterator operator++(int) {
        //         iterator tmp = *this;
        //         nodeT* new_ptr = _ptr->next();
        //         if (_dummy_ptr and new_ptr == _dummy_ptr) {
        //             new_ptr = new_ptr->next();
        //         }
        //         _mempool->sync(_ptr);
        //         _ptr = new_ptr;
        //         return tmp;
        //     }

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

    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
