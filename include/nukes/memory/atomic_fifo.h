#ifndef NUKES_MEMORY_ATOMIC_FIFO
#define NUKES_MEMORY_ATOMIC_FIFO

#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"


namespace nukes::memory {


template <typename dataT, details::constants::hword lenV = details::constants::runtime_discover>
struct atomic_fifo {

protected:

    typedef details::nodes::mem_node<dataT> chunk_node_t;
    typedef details::nodes::stc_node_hdr node_hdr_t;
    typedef typename chunk_node_t::atomic_t atomic_node_hdr_t;

    static constexpr std::size_t storage_size_v  {
        lenV
            ? lenV * sizeof(chunk_node_t)
            : sizeof(chunk_node_t)
    };
    typedef details::misc::meta_data<storage_size_v> storage_t;

    alignas(8) storage_t             _storage {};
    alignas(8) chunk_node_t*         _buffer  { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::hword  _len     { lenV };
    alignas(64) atomic_node_hdr_t    _head    {};           // NOTE: Квази-указатель головы
    alignas(64) atomic_node_hdr_t    _tail    {};           // NOTE: Квази-указатель хвоста

public:

    atomic_fifo() noexcept
        requires ( lenV != details::constants::runtime_discover );

    explicit atomic_fifo(details::constants::hword) noexcept
        requires ( lenV == details::constants::runtime_discover );

    ~atomic_fifo() noexcept =default;

    // NOTE: Выдача указателя на сегмент по индексу буфера
    [[nodiscard]] dataT* ptr_by_idx(details::constants::hword idx) noexcept;

    // NOTE: Выдача индекса в буфере по указателю на объект
    [[nodiscard]] details::constants::hword idx_by_ptr(dataT* ptr) const noexcept;

    // NOTE: Освобождение сегмента по индексу
    bool sync_idx(details::constants::hword& idx) noexcept;

    // NOTE: Захват сегмента по индексу
    bool capture_idx(details::constants::hword& idx) noexcept;

    // NOTE: Освобождение переданного сегмента
    bool sync(dataT*& ptr) noexcept;

    // NOTE: Захват свободного сегмента
    bool capture(dataT*& ptr) noexcept;

    // NOTE: Освобождение переданного сегмента
    bool empty() noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_FIFO_MEMBER(member_type)                              \
    template <typename dataT, nukes::details::constants::hword lenV>    \
    member_type nukes::memory::atomic_fifo<dataT, lenV>::


ATOMIC_FIFO_MEMBER()
atomic_fifo() noexcept
requires ( lenV != details::constants::runtime_discover ) {

    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<chunk_node_t>()) chunk_node_t[_len];
    node_hdr_t next {._node_idx = 0, ._tag = 0};
    _head.store(next);
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = static_cast<details::constants::hword>(i + 1);
        _buffer[i]._next.store(next);
    }
    _tail.store(next);
}

ATOMIC_FIFO_MEMBER()
atomic_fifo(const details::constants::hword l) noexcept
requires ( lenV == details::constants::runtime_discover )
: _len(l) {

    // NOTE: При динамическом определении размера, выделяем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _storage = new chunk_node_t[_len];
    _buffer = _storage.template release<chunk_node_t*>();
    node_hdr_t next {._node_idx = 0, ._tag = 0};
    _head.store(next);
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = static_cast<details::constants::hword>(i + 1);
        _buffer[i]._next.store(next);
    }
    _tail.store(next);
}


ATOMIC_FIFO_MEMBER(dataT*)
ptr_by_idx(details::constants::hword idx) noexcept {
    return idx < _len ? &(_buffer[idx]._mem) : nullptr;
}


ATOMIC_FIFO_MEMBER(nukes::details::constants::hword)
idx_by_ptr(dataT* ptr) const noexcept {

    // NOTE: Вычитаем из абсолютного адреса смещение до адреса начала буфера
    //       и размер квази-указателя на следующий элемент из типа узла,
    //       т.к при захвате данных выдаём указатель на память под объект, а узлы буфера хранят:
    //       (квази-указатель на следующий элемент + память под объект)
    const details::constants::word normalized_addr {
        reinterpret_cast<details::constants::word>(ptr)
        - reinterpret_cast<details::constants::word>(&_buffer[0])
        - sizeof(typename chunk_node_t::atomic_t)
    };

    // NOTE: Делим адрес на размер объекта буфера, получаем индекс
    const details::constants::hword idx {
        static_cast<details::constants::hword>(normalized_addr / sizeof(chunk_node_t))
    };

    // NOTE: Проверяем что индекс не вышел за размер буфера
    return idx < _len ? idx : details::constants::hword_max_v;
}


ATOMIC_FIFO_MEMBER(bool)
sync_idx(details::constants::hword& idx) noexcept {

    // NOTE: Выходим если индекс превышает размер буфера
    if (idx >= _len) [[unlikely]]
        return false;

    // NOTE: Создаём сущности нового узла хвоста и копию текущего хвоста
    node_hdr_t new_tail_hdl, tail_hdl = _tail.load();
    // NOTE: Устанавливаем индекс нового хвоста равный передаваемому индексу для высвобождения
    new_tail_hdl._node_idx = idx;

    // NOTE: Вставляем dummy в next, который будет перезаписан при следующем sync
    static constexpr node_hdr_t dummy {
        ._node_idx = details::constants::hword_max_v,
        ._tag = lenV
    };
    _buffer[idx]._next.store(dummy);

    do {
        // NOTE: Новый хвост должен иметь больше касаний чем старый
        new_tail_hdl._tag = tail_hdl._tag + 1;
        // NOTE: В новом узле приращаем тег, и проводим замену только в том случае,
        //       в голову пишется новый узел, указывающий
        //       на текущую голову как на следующий узел
    } while (not _tail.compare_exchange_weak(tail_hdl, new_tail_hdl,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

    // NOTE: Старый хвост должен указывать на новый как на следующий
    _buffer[tail_hdl._node_idx]._next.store(new_tail_hdl);

    // NOTE: Портим переданный индекс чтобы им было нельзя воспользоваться снова
    idx = details::constants::hword_max_v;
    return true;
}


ATOMIC_FIFO_MEMBER(bool)
capture_idx(details::constants::hword& idx) noexcept {

    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    node_hdr_t new_head_hdl, head_hdl = _head.load();

    // NOTE: После каждой неудачной попытки замещения
    //       top_hdl будет обновляться текущей головой
    //       средствами compare_exchange_weak(...)
    do {
        new_head_hdl = _buffer[head_hdl._node_idx]._next.load();
        if (new_head_hdl._node_idx == details::constants::hword_max_v) [[unlikely]]
            return false;

        // NOTE: Новая голова должна иметь больше касаний чем старая
        new_head_hdl._tag = head_hdl._tag + 1;

        // NOTE: Проводим замену только в том случае,
        //       если тег и индекс текущей головы остался неизменен,
        //       в голову пишется следующий за ней узел
    } while (not _head.compare_exchange_weak(head_hdl, new_head_hdl));

    idx = head_hdl._node_idx;
    return true;
}

ATOMIC_FIFO_MEMBER(bool)
sync(dataT*& ptr) noexcept {


    // NOTE: Получаем индекс по указателю
    details::constants::hword idx = idx_by_ptr(ptr);
    // NOTE: Портим указатель, чтобы им нельзя было
    //       пользоваться повторно
    ptr = nullptr;

    // NOTE: Используем Освобождение по индексу
    return idx < _len and sync_idx(idx);
}


ATOMIC_FIFO_MEMBER(bool)
capture(dataT*& ptr) noexcept {

    // NOTE: Индекс захваченной памяти
    details::constants::hword idx {0};

    // NOTE: Используем захват по индексу и каст индекса в указатель
     return capture_idx(idx) and (ptr = ptr_by_idx(idx)) not_eq nullptr;
}


ATOMIC_FIFO_MEMBER(bool)
empty() noexcept {

    auto head = _head.load(std::memory_order_acquire);
    head._tag = _tail.load()._tag;
    if (_tail.compare_exchange_weak(head, head, std::memory_order_release, std::memory_order_relaxed))
        return true;
    return false;
}


#undef ATOMIC_FIFO_MEMBER
#endif // NUKES_MEMORY_ATOMIC_FIFO
