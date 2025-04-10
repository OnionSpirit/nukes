#ifndef NUKES_DETAILS_BATCH
#define NUKES_DETAILS_BATCH

#include <concepts>


namespace nukes::details {

    template <typename nodeT, typename mempoolT>
    class batch {

        static constexpr bool node_t_assert = requires(nodeT node) {
            { node.next() } -> std::same_as<nodeT*>;
        };

        static constexpr bool mempool_t_assert = requires(nodeT *&node, mempoolT mempool) {
            { mempool.sync(node) } -> std::same_as<bool>;
        };

        static_assert(node_t_assert, "method 'next()' required for 'nodeT' for iterating");

        static_assert(mempool_t_assert, "method 'sync(node*&)' required for 'mempoolT' for memory management");

        nodeT* _head;
        nodeT* _tail;

        mempoolT* _mempool;

        class iterator {

            nodeT* _ptr;
            mempoolT* _mempool;

        public:
            explicit iterator(nodeT* ptr, mempoolT mempool) : _ptr(ptr), _mempool(mempool) {}

            nodeT& operator*() const { return *_ptr; }
            nodeT* operator->() { return _ptr; }

            iterator& operator++() {
                _ptr = _ptr->next();
                return *this;
            }

            iterator operator++(int) {
                iterator tmp = *this;
                _ptr =_ptr->next();
                return tmp;
            }

            bool operator==(const iterator& other) const { return _ptr == other._ptr; }
            bool operator!=(const iterator& other) const { return _ptr != other._ptr; }

            ~iterator() { _mempool->sync(_ptr); }
        };

    public:

        iterator begin() { return iterator(_head, _mempool); }
        iterator end() { return iterator(_tail, _mempool); }

        iterator begin() const { return iterator(_head, _mempool); }
        iterator end() const { return iterator(_tail, _mempool); }

    };

} // end namespace nukes

#endif // NUKES_DETAILS_BATCH
