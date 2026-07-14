#ifndef NUKES_DETAILS_BATCH
#define NUKES_DETAILS_BATCH

#include <concepts>


namespace nukes::details {

    template <typename nodeT, typename iteratorT>
    concept is_batch_iterable = requires(nodeT*& node, iteratorT iter) {
        { iter.prefix_increment(node) } -> std::same_as<iteratorT&>;
        { iter.postfix_increment(node) } -> std::same_as<iteratorT>;
    };

    template<typename nodeT, class iteratorImpl, typename batch_producer_t>
    class batch_iterator_traits {

        nodeT* _ptr;
        iteratorImpl _iter;

        static_assert(is_batch_iterable<nodeT, iteratorImpl>,
            "Iterator implementation is not actually an iterator");

        // static_assert(std::constructible_from<iteratorImpl, batch_producer_t>,
        //     "Passed iterator can't be constructed from passed args");

    public:

        explicit batch_iterator_traits(nodeT* ptr, batch_producer_t* producer)
            : _ptr(ptr)
            , _iter(std::forward<batch_producer_t*>(producer)) {}

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

    template <typename nodeT, class iteratorT, typename batch_producer_t>
    class batch {

        nodeT* _head;
        nodeT* _tail;
        batch_producer_t* _producer{};

    public:

        explicit batch() =default;

        explicit batch(nodeT* head, nodeT* tail, batch_producer_t* producer)
            : _head(head)
            , _tail(tail)
            , _producer(producer) {}

        typedef batch_iterator_traits<nodeT, iteratorT, batch_producer_t> iterator_t;

        iterator_t begin() { return iterator_t(_head, std::forward<batch_producer_t*>(_producer)); }
        iterator_t end() { return iterator_t(_tail, std::forward<batch_producer_t*>(_producer)); }

        iterator_t begin() const { return iterator_t(_head, std::forward<batch_producer_t*>(_producer)); }
        iterator_t end() const { return iterator_t(_tail, std::forward<batch_producer_t*>(_producer)); }
    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
