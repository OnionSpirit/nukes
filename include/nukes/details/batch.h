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

    // NOTE: A helper for correct recycling of the dummy node from the batch consumer
    struct recycle_helper {

        enum class result : char {
            e_dummy_reached,
            e_tail_reached,
            e_none
        };

    private:

        void* _producer {};
        void* _dummy {};
        void* _tail {};
        result (*_recycle_callback)(void*, void*, void*, void*) {};

        template <typename producer_t>
        static auto recycle_impl(void* producer, void* tail, void* dummy, void* candidate) -> result {
            if (candidate == tail and candidate != dummy)
                return result::e_tail_reached;

            auto* real_producer = static_cast<producer_t*>(producer);
            auto* real_dummy = static_cast<producer_t::node_t*>(candidate);
            if (real_producer->recycle_dummy(real_dummy))
                return result::e_dummy_reached;
            return result::e_none;

        }

    public:

        recycle_helper() = delete;

        recycle_helper(const recycle_helper&) = default;

        recycle_helper& operator=(const recycle_helper&) = default;

        template <typename producer_t>
        recycle_helper(producer_t* producer, void* dummy, void* tail) {
            _producer = producer;
            _dummy = dummy;
            _tail = tail;
            _recycle_callback = recycle_impl<producer_t>;
        }

        [[nodiscard]] result recycle(void* candidate) {
            const result res = _recycle_callback(_producer, _tail, _dummy, candidate);
            if (res != result::e_none) {
                _producer = _dummy = _tail = nullptr;
                _recycle_callback = nullptr;
            }
            return res;
        }

        ~recycle_helper() {
            _tail = _dummy = _producer = nullptr;
            _recycle_callback = nullptr;
        }

    };

    template <typename nodeT, class iteratorT, typename batch_producer_t>
    class batch {

        nodeT* _head;
        nodeT* _tail;
        nodeT* _dummy;
        batch_producer_t* _producer{};

    public:

        explicit batch() =default;

        explicit batch(nodeT* head, nodeT* tail, nodeT* dummy, batch_producer_t* producer)
            : _head(head)
            , _tail(tail)
            , _dummy(dummy)
            , _producer(producer) {}

        typedef batch_iterator_traits<nodeT, iteratorT, batch_producer_t> iterator_t;

        iterator_t begin() { return iterator_t(_head, std::forward<batch_producer_t*>(_producer)); }
        iterator_t end() { return iterator_t(_tail, std::forward<batch_producer_t*>(_producer)); }

        iterator_t begin() const { return iterator_t(_head, std::forward<batch_producer_t*>(_producer)); }
        iterator_t end() const { return iterator_t(_tail, std::forward<batch_producer_t*>(_producer)); }

        auto enlist_data() {
            auto recycler = recycle_helper{_producer, _dummy, _tail};
            return std::tie(_head, _tail, recycler);
        }
    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
