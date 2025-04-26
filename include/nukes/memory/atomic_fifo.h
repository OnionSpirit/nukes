#ifndef NUKES_MEMORY_ATOMIC_FIFO
#define NUKES_MEMORY_ATOMIC_FIFO

#include <atomic>
#include <cstdint>
#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"



namespace nukes::memory {

template <typename dataT, details::constants::hword lenV = 1024>
struct atomic_fifo {

protected:

    typedef details::nodes::mem_node<dataT> chunk_node_t;
    typedef details::nodes::stc_node_hdr node_hdr_t;
    typedef chunk_node_t::atomic_t atomic_node_hdr_t;
    typedef details::misc::meta_data<lenV * sizeof(chunk_node_t)> storage_t;

    alignas(8) storage_t             _storage {};
    alignas(8) chunk_node_t*         _buffer  { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::hword  _len     { lenV };
    alignas(64) atomic_node_hdr_t    _head    {};           // NOTE: Квази-указатель головы
    alignas(64) atomic_node_hdr_t    _tail    {};           // NOTE: Квази-указатель хвоста

public:

    atomic_fifo() noexcept
        requires ( lenV != details::constants::runtime_discover );

    atomic_fifo(details::constants::hword) noexcept
        requires ( lenV == details::constants::runtime_discover );

    ~atomic_fifo() noexcept =default;

    // NOTE: Выдача указателя на чанк по индексу буфера
    [[nodiscard]] dataT* ptr_by_idx(details::constants::hword idx) noexcept;

    // NOTE: Выдача индекса в буфере по указателю на объект
    [[nodiscard]] details::constants::hword idx_by_ptr(dataT* ptr) const noexcept;

    // NOTE: Освобождение чанка по индексу
    [[nodiscard]] bool sync_idx(details::constants::hword& idx) noexcept;

    // NOTE: Захват чанка по индексу
    [[nodiscard]] bool capture_idx(details::constants::hword& idx) noexcept;

    // NOTE: Освобождение переданного чанка
    [[nodiscard]] bool sync(dataT*& ptr) noexcept;

    // NOTE: Захват свободного чанка
    [[nodiscard]] bool capture(dataT*& ptr) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_FIFO_MEMBER(member_type)                              \
    template <typename dataT, nukes::details::constants::hword lenV>    \
    member_type nukes::memory::atomic_fifo<dataT, lenV>::


ATOMIC_FIFO_MEMBER()
atomic_fifo() noexcept
requires ( lenV != nukes::details::constants::runtime_discover ) {

    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<chunk_node_t>()) chunk_node_t[_len];
    node_hdr_t next {._node_idx = 0, ._tag = 0};
    _head.store(next);
    _tail.store({._node_idx = _len, ._tag = 0});
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = (nukes::details::constants::hword)(i + 1);
        _buffer[i]._next.store(next);
    }
}

ATOMIC_FIFO_MEMBER()
atomic_fifo(nukes::details::constants::hword l) noexcept
requires(lenV == nukes::details::constants::runtime_discover)
: _len(l) {

    // NOTE: При динамическом определении размера, аллоцируем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _buffer = (_storage = new chunk_node_t[_len]).template release<chunk_node_t*>();
    node_hdr_t next {._node_idx = 0, ._tag = 0};
    _head.store(next);
    _tail.store({._node_idx = _len, ._tag = 0});
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = (nukes::details::constants::hword)(i + 1);
        _buffer[i]._next.store(next);
    }
}


ATOMIC_FIFO_MEMBER(dataT*)
ptr_by_idx(nukes::details::constants::hword idx) noexcept {
    return idx < _len ? &(_buffer[idx]._mem) : nullptr;
}


ATOMIC_FIFO_MEMBER(nukes::details::constants::hword)
idx_by_ptr(dataT* ptr) const noexcept {

    // NOTE: Вычитаем из абсолютного адреса смещение до адреса начала буфера
    //       и размер квази-указателя на следующий элемент из типа узла,
    //       т.к. при захвате данных выдаём указатель на память под объект, а узлы буфера хранят:
    //       (квази-указатель на следующий элемент + память под объект)
    const nukes::details::constants::word normalized_addr
        { ((nukes::details::constants::word)ptr
           - (nukes::details::constants::word)&_buffer[0]
           - sizeof(typename nukes::details::nodes::mem_node<dataT>::atomic_t)) };

    // NOTE: Делим адрес на размер объекта буффера, получаем индекс
    const nukes::details::constants::hword idx
        { static_cast<nukes::details::constants::hword>(normalized_addr / sizeof(chunk_node_t)) };

    // NOTE: Проверяем что индекс не вышел за размер буффера
    return idx < _len ? idx : UINT32_MAX;
}


ATOMIC_FIFO_MEMBER(bool)
sync_idx(nukes::details::constants::hword &idx) noexcept {

    // NOTE: Выходим если индекс превышает размер буфера
    if (idx >= _len) [[unlikely]] return false;

    // NOTE: Создаём сущности нового узла хвоста и копию текущего хвоста
    node_hdr_t new_tail_hdl, head_hdl, tail_hdl = _tail.load();
    // Устанавливаем индекс нового хвоста равный передаваемому индексу для высвобождения
    new_tail_hdl._node_idx = idx;

    do {
        // NOTE: Новый хвост должен иметь больше касаний чем старый
        new_tail_hdl._tag = tail_hdl._tag + 1;
        // NOTE: В новом узле приращаем тег, и проводим замену только в том случае,
        //       в голову пишется новый узел, указывающий
        //       на текущую голову как на следующий узел
    } while (not _head.compare_exchange_weak(tail_hdl, new_tail_hdl,
                                             std::memory_order_release,
                                             std::memory_order_relaxed));

    // NOTE: Старый хвост должен указывать на новый как на следующий
    _buffer[tail_hdl._node_idx]._next.store(new_tail_hdl);

    // NOTE: Портим переданный индекс чтобы им было нельзя воспользоваться снова
    idx = UINT32_MAX;
    return true;
}


ATOMIC_FIFO_MEMBER(bool)
capture_idx(nukes::details::constants::hword& idx) noexcept {

    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    node_hdr_t new_head_hdl, head_hdl = _head.load();

    // NOTE: После каждой неудачной попытки замещения
    //       top_hdl будет обновляться текущей головой
    //       средствами compare_exchange_weak(...)
    do { if (head_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;
        // NOTE: Новая голова должна иметь больше касаний чем старая
        new_head_hdl._tag
            = head_hdl._tag + 1;
        // NOTE: Новая голова должна иметь индекс буфера
        //       равный индексу следующего за головой узла
        new_head_hdl._node_idx
            = _buffer[head_hdl._node_idx]._next.load()._node_idx;
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
    nukes::details::constants::hword idx = idx_by_ptr(ptr);

    // NOTE: Используем Освобождение по индексу
    const bool is_synced
        {idx < _len and sync_idx(idx) };

    if (is_synced) [[likely]] {
        // NOTE: Портим указатель, чтобы им нельзя было пользоваться повторно
        ptr = nullptr;
        return true;
    }

    // NOTE: Если не вышли раньше значит ошибка
    return false;
}


ATOMIC_FIFO_MEMBER(bool)
capture(dataT*& ptr) noexcept {

    // NOTE: Индекс захваченой памяти
    nukes::details::constants::hword idx {0};

    // NOTE: Используем захват по индексу
    const bool is_captured

        { capture_idx(idx) and idx < _len};
    if (is_captured) [[likely]] {
        // NOTE: Выдаём указатель на данные уже вытащенной головы
        ptr = &(_buffer[idx]._mem);
        return true;
    }

    // NOTE: Если не вышли раньше значит ошибка
    return false;
}


#undef ATOMIC_FIFO_MEMBER
#endif // NUKES_MEMORY_ATOMIC_FIFO
