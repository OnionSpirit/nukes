#ifndef ATOMIC_STACK
#define ATOMIC_STACK

#include <cstdint>
#include <atomic>
#include <iostream>
#include <sys/types.h>
#include <thread>
#include <type_traits>
#include <utility>



/// =============================== A-B-A problem classic stack ===============================

template <typename dataT>
struct aba_node {
    std::atomic<aba_node*> _next {};
    dataT _data {};
};


template <typename dataT>
class aba_atomic_stack_base {

protected:
    
    typedef aba_node<dataT> node_t;

    std::atomic<node_t*> _top {};

public:

    bool push_new(nukes::details::misc::fn_forward_t<dataT> data) {

        node_t* new_node = new node_t();
        new_node->_data = std::forward<dataT>(data);

        node_t* top_hdl = _top.load();
        new_node->_next.store(top_hdl);
        
        do new_node->_next.store(top_hdl);
        while (not _top.compare_exchange_weak(top_hdl, new_node));
        
        return true;        

    }
        
    bool pop_new(dataT& data) {

        node_t* top_hdl = _top.load();
        node_t* new_top_hdl = top_hdl
            ? top_hdl->_next.load()
            : nullptr;

        do new_top_hdl = top_hdl
            ? top_hdl->_next.load()
            : nullptr;
        while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

        if (top_hdl) {
            data = top_hdl->_data;
            delete top_hdl;
            return true;
        } else return false;

    }
 
};


#endif // ATOMIC_STACK
