#ifndef NUKES_POOL_ATOMIC_LIFO
#define NUKES_POOL_ATOMIC_LIFO

#include <atomic>
#include <cstdint>
#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"


namespace nukes::pool {


template <typename dataT, uint32_t lenV = 1024>
struct atomic_lifo {

protected:


    typedef details::nodes::mem_node<dataT> chunk_node_t;
    typedef std::atomic<details::nodes::stc_node_hdl> node_hdl_t;
    typedef details::misc::meta_data<lenV * sizeof(chunk_node_t)> storage_t;

    node_hdl_t      _top     {};           // NOTE: Квази-указатель вершины
    chunk_node_t*   _buffer  { nullptr };  // NOTE: Буфер хранения данных
    const uint32_t  _len     { lenV };
    storage_t       _storage { };

public:

    atomic_lifo() noexcept
        requires ( lenV != details::constants::runtime_discover );

    atomic_lifo(uint32_t) noexcept
        requires ( lenV == details::constants::runtime_discover );

    ~atomic_lifo() noexcept =default;

    // NOTE: Выдача указателя на чанк по индексу буфера
    [[nodiscard]] dataT* ptr_by_idx(uint32_t idx) noexcept;

    // NOTE: Выдача индекса в буфере по указателю на объект
    [[nodiscard]] uint32_t idx_by_ptr(dataT* ptr) const noexcept;

    // NOTE: Освобождение чанка по индексу
    [[nodiscard]] bool sync_idx(uint32_t& idx) noexcept;

    // NOTE: Захват чанка по индексу
    [[nodiscard]] bool capture_idx(uint32_t& idx) noexcept;

    // NOTE: Освобождение переданного чанка
    [[nodiscard]] bool sync(dataT*& ptr) noexcept;

    // NOTE: Захват свободного чанка
    [[nodiscard]] bool capture(dataT*& ptr) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_LIFO_MEMBER(member_type)                 \
    template <typename dataT, uint32_t lenV>            \
    member_type nukes::pool::atomic_lifo<dataT, lenV>::


ATOMIC_LIFO_MEMBER()
atomic_lifo() noexcept
requires ( lenV != details::constants::runtime_discover ) {

    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<chunk_node_t>()) chunk_node_t[_len];
    details::nodes::stc_node_hdl next {._node_idx = 0, ._tag = 0};
    _top.store(next);
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = (uint32_t)(i + 1);
        _buffer[i]._next.store(next);
    }
}

ATOMIC_LIFO_MEMBER()
atomic_lifo(uint32_t l) noexcept
requires(lenV == details::constants::runtime_discover)
: _len(l) {

    // NOTE: При динамическом определении размера, аллоцируем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер 
    _buffer = (_storage = new chunk_node_t[_len]).template release<chunk_node_t*>();
    details::nodes::stc_node_hdl next {._node_idx = 0, ._tag = 0};
    _top.store(next);
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < _len - 1; ++i) {
        next._node_idx = (uint32_t)(i + 1);
        _buffer[i]._next.store(next);
    }
}


ATOMIC_LIFO_MEMBER(dataT*)
ptr_by_idx(uint32_t idx) noexcept {
    return idx < _len ? &(_buffer[idx]._mem) : nullptr;
}


ATOMIC_LIFO_MEMBER(uint32_t)
idx_by_ptr(dataT* ptr) const noexcept {

    // NOTE: Вычитаем из абсолютного адреса смещение до адреса начала буфера
    //       и размер квази-указателя на следующий элемент из типа узла,
    //       т.к. при захвате данных выдаём указатель на память под объект, а узлы буфера хранят:
    //       (квази-указатель на следующий элемент + память под объект)
    const uint64_t normalized_addr
        { ((uint64_t)ptr
           - (uint64_t)&_buffer[0]
           - sizeof(typename details::nodes::mem_node<dataT>::atomic_t)) };

    // NOTE: Делим адрес на размер объекта буффера, получаем индекс
    const uint32_t idx
        { static_cast<uint32_t>(normalized_addr / sizeof(chunk_node_t)) };

    // NOTE: Проверяем что индекс не вышел за размер буффера
    return idx < _len ? idx : UINT32_MAX;
}


ATOMIC_LIFO_MEMBER(bool)
sync_idx(uint32_t &idx) noexcept {

    // NOTE: Выходим если индекс превышает размер буфера
    if (idx >= _len) [[unlikely]] return false;

    // NOTE: Указатель освобождаемую память в буфере
    chunk_node_t* new_node = &_buffer[idx];
    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    details::nodes::stc_node_hdl new_top_hdl, top_hdl = _top.load();
    // Устанавливаем индекс новой головы равный передаваемому индексу для высвобождения
    new_top_hdl._node_idx = idx;

    // NOTE: После каждой неудачной попытки замещения
    //       top_hdl будет обновляться текущей головой
    //       средствами compare_exchange_weak(...)
    do {
        // NOTE: Новая голова должна иметь больше касаний чем старая
        new_top_hdl._tag = top_hdl._tag + 1;
        // NOTE: Новая голова должна указывать на старую как на следующую
        new_node->_next.store(top_hdl);
        // NOTE: В новом узле приращаем тег, и проводим замену только в том случае,
        //       в голову пишется новый узел, указывающий
        //       на текущую голову как на следующий узел
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    // NOTE: Портим переданный индекс чтобы им было нельзя воспользоваться снова
    idx = UINT32_MAX;
    return true;
}


ATOMIC_LIFO_MEMBER(bool)
capture_idx(uint32_t& idx) noexcept {

    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    details::nodes::stc_node_hdl new_top_hdl, top_hdl = _top.load();

    // NOTE: После каждой неудачной попытки замещения
    //       top_hdl будет обновляться текущей головой
    //       средствами compare_exchange_weak(...)
    do { if (top_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;
        // NOTE: Новая голова должна иметь больше касаний чем старая
        new_top_hdl._tag
            = top_hdl._tag + 1;
        // NOTE: Новая голова должна иметь индекс буфера
        //       равный индексу следующего за головой узла
        new_top_hdl._node_idx
            = _buffer[top_hdl._node_idx]._next.load()._node_idx;
        // NOTE: Проводим замену только в том случае,
        //       если тег и индекс текущей головы остался неизменен,
        //       в голову пишется следующий за ней узел
    } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    idx = top_hdl._node_idx;
    return true;
}

ATOMIC_LIFO_MEMBER(bool)
sync(dataT*& ptr) noexcept {


    // NOTE: Получаем индекс по указателю
    uint32_t idx = idx_by_ptr(ptr);

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


ATOMIC_LIFO_MEMBER(bool)
capture(dataT*& ptr) noexcept {

    // NOTE: Индекс захваченой памяти
    uint32_t idx {0};

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


#undef ATOMIC_LIFO_MEMBER
#endif // NUKES_POOL_ATOMIC_LIFO
