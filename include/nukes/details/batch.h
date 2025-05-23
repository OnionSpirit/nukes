#ifndef NUKES_DETAILS_BATCH
#define NUKES_DETAILS_BATCH

#include <concepts>


namespace nukes::details {

    template <typename nodeT, typename mempoolT>
    class batch {

        static constexpr bool node_t_assert = requires(nodeT node) {
            node.next();
            node._data;
        };

        static constexpr bool mempool_t_assert = requires(nodeT *&node, mempoolT mempool) {
            { mempool.sync(node) } -> std::same_as<bool>;
        };

        static_assert(node_t_assert, "field '_next' required for 'nodeT' for iterating");

        static_assert(mempool_t_assert, "method 'sync(node*&)' required for 'mempoolT' for memory management");

        nodeT* _head;
        nodeT* _tail;

        mempoolT* _mempool;

        class iterator {

            nodeT* _ptr;
            mempoolT* _mempool;

        public:
            explicit iterator(nodeT* ptr, mempoolT* mempool) : _ptr(ptr), _mempool(mempool) {}

            auto& operator*() const { return _ptr->_data; }
            nodeT* operator->() { return _ptr; }

            iterator& operator++() {
                nodeT* new_ptr = _ptr->next();
                _mempool->sync(_ptr);
                _ptr = new_ptr;
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                nodeT* new_ptr = _ptr->next();
                _mempool->sync(_ptr);
                _ptr = new_ptr;
                return tmp;
            }

            bool operator==(const iterator& other) const { return _ptr == other._ptr; }
            bool operator!=(const iterator& other) const { return _ptr != other._ptr; }
        };

    public:

        explicit batch() =default;

        explicit batch(nodeT* head, nodeT* tail, mempoolT* mempool)
            : _head(head)
            , _tail(tail)
            , _mempool(mempool) {}

        iterator begin() { return iterator(_head, _mempool); }
        iterator end() { return iterator(_tail, _mempool); }

        iterator begin() const { return iterator(_head, _mempool); }
        iterator end() const { return iterator(_tail, _mempool); }

    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
