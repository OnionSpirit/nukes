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
        nodeT* _dummy_ptr { nullptr };
        mempoolT* _mempool;

        class iterator {

            nodeT* _ptr;
            mempoolT* _mempool;
            nodeT* _dummy_ptr { nullptr };

        public:
            explicit iterator(nodeT* ptr, nodeT* dummy, mempoolT* mempool)
                : _ptr(ptr)
                , _mempool(mempool)
                , _dummy_ptr(dummy) {}

            auto& operator*() const { return _ptr->_data; }
            nodeT* operator->() { return _ptr; }

            iterator& operator++() {
                nodeT* new_ptr = _ptr->next();
                if (_dummy_ptr and new_ptr == _dummy_ptr) {
                    new_ptr = new_ptr->next();
                }
                _mempool->sync(_ptr);
                _ptr = new_ptr;
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                nodeT* new_ptr = _ptr->next();
                if (_dummy_ptr and new_ptr == _dummy_ptr) {
                    new_ptr = new_ptr->next();
                }
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

        explicit batch(nodeT* head, nodeT* tail, nodeT* dummy, mempoolT* mempool)
            : _head(head)
            , _tail(tail)
            , _dummy_ptr(dummy)
            , _mempool(mempool) {}

        iterator begin() { return iterator(_head, _dummy_ptr, _mempool); }
        iterator end() { return iterator(_tail, _dummy_ptr, _mempool); }

        iterator begin() const { return iterator(_head, _dummy_ptr, _mempool); }
        iterator end() const { return iterator(_tail, _dummy_ptr, _mempool); }

    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
