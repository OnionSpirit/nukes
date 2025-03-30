#ifndef NUKES_ATOMIC_BOUNDED_FREELIST
#define NUKES_ATOMIC_BOUNDED_FREELIST

#include <atomic>
#include <cstdint>

#include "constants.h"
#include "helpers.h"
#include "node_types.h"
#include "meta.h"



namespace nukes {


template <typename ChunkType, uint32_t ssize = 1024>
struct atomic_bounded_freelist {

protected:

    typedef mem_node<ChunkType> chunk_node_t;
    typedef std::atomic<stc_node_hdl> node_hdl_t;

    node_hdl_t   _top           {}; // NOTE: Квази-указатель вершины
    chunk_node_t _buffer[ssize] {}; // NOTE: Буфер хранения данных

public:

    atomic_bounded_freelist() noexcept;

    ~atomic_bounded_freelist() noexcept =default;

    // NOTE: Выдача указателя на чанк по индексу буфера
    [[nodiscard]] ChunkType* ptr_by_idx(uint32_t idx) noexcept;

    // NOTE: Выдача индекса в буфере по указателю на объект
    [[nodiscard]] uint32_t idx_by_ptr(ChunkType* ptr) const noexcept;

    // NOTE: Освобождение чанка по индексу
    [[nodiscard]] bool sync_idx(uint32_t& idx) noexcept;

    // NOTE: Захват чанка по индексу
    [[nodiscard]] bool capture_idx(uint32_t& idx) noexcept;

    // NOTE: Освобождение переданного чанка
    [[nodiscard]] bool sync(ChunkType*& ptr) noexcept;

    // NOTE: Захват свободного чанка
    [[nodiscard]] bool capture(ChunkType*& ptr) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================


ATOMIC_BOUNDED_FREELIST_MEMBER()
atomic_bounded_freelist() noexcept {
    stc_node_hdl next {._node_idx = 0, ._tag = 0};
    _top.store(next);
    // NOTE: Связывание узлов в буфере
    for (int i =0; i < ssize - 1; ++i) {
        next._node_idx = (uint32_t)(i + 1);
        _buffer[i]._next.store(next);
    }
}


ATOMIC_BOUNDED_FREELIST_MEMBER(ChunkType*)
ptr_by_idx(uint32_t idx) noexcept {
    return idx < ssize ? &(_buffer[idx]._mem) : nullptr;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(uint32_t)
idx_by_ptr(ChunkType* ptr) const noexcept {

    // NOTE: Вычитаем из абсолютного адреса смещение до адреса начала буфера
    //       и размер квази-указателя на следующий элемент из типа узла,
    //       т.к. при захвате данных выдаём указатель на память под объект, а узлы буфера хранят:
    //       (квази-указатель на следующий элемент + память под объект)
    const uint64_t normalized_addr
        { ((uint64_t)ptr - (uint64_t)&_buffer[0] - sizeof(typename mem_node<ChunkType>::atomic_t)) };

    // NOTE: Делим адрес на размер объекта буффера, получаем индекс
    const uint32_t idx
        { static_cast<uint32_t>(normalized_addr / sizeof(chunk_node_t)) };

    // NOTE: Проверяем что индекс не вышел за размер буффера
    return idx < ssize ? idx : UINT32_MAX;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
sync_idx(uint32_t &idx) noexcept {

    // NOTE: Выходим если индекс превышает размер буфера
    if (idx >= ssize) [[unlikely]] return false;

    // NOTE: Указатель освобождаемую память в буфере
    chunk_node_t* new_node = &_buffer[idx];
    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    stc_node_hdl new_top_hdl, top_hdl = _top.load();
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


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
capture_idx(uint32_t& idx) noexcept {

    // NOTE: Создаём сущности нового узла головы и копию текущей головы
    stc_node_hdl new_top_hdl, top_hdl = _top.load();

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

ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
sync(ChunkType*& ptr) noexcept {


    // NOTE: Получаем индекс по указателю
    uint32_t idx = idx_by_ptr(ptr);

    // NOTE: Используем Освобождение по индексу
    const bool is_synced
        {idx < ssize and sync_idx(idx) };

    if (is_synced) {
        // NOTE: Портим указатель, чтобы им нельзя было пользоваться повторно
        ptr = nullptr;
        return true;
    }

    // NOTE: Если не вышли раньше значит ошибка
    return false;

    // // NOTE: Получаем индекс по указателю
    // const uint64_t idx = idx_by_ptr(ptr);

    // // NOTE: Если индекс больше размера возвращаем ошибку
    // if (idx >= ssize) [[unlikely]] return false;

    // // NOTE: Указатель освобождаемую память в буфере
    // chunk_node_t* new_node = &_buffer[idx];
    // // NOTE: Создаём сущности нового узла головы и копию текущей головы
    // stc_node_hdl new_top_hdl, top_hdl = _top.load();
    // // Устанавливаем индекс новой головы равный передаваемому индексу для высвобождения
    // new_top_hdl._node_idx = idx;

    // // NOTE: После каждой неудачной попытки замещения
    // //       top_hdl будет обновляться текущей головой
    // //       средствами compare_exchange_weak(...)
    // do {
    //     // NOTE: Новая голова должна иметь больше касаний чем старая
    //     new_top_hdl._tag = top_hdl._tag + 1;
    //     new_node->_next.store(top_hdl);
    //     // NOTE: Проводим замену только в том случае,
    //     //       если тег и индекс текущей головы остался неизменен,
    //     //       в голову пишется новый узел, указывающий
    //     //       на текущую голову как на следующий узел
    // } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    // // NOTE: Портим указатель, чтобы им нельзя было пользоваться повторно
    // ptr = nullptr;
    // return true;
}


ATOMIC_BOUNDED_FREELIST_MEMBER(bool)
capture(ChunkType*& ptr) noexcept {

    // NOTE: Индекс захваченой памяти
    uint32_t idx {0};

    // NOTE: Используем захват по индексу
    const bool is_captured
        { capture_idx(idx) and idx < ssize};

    if (is_captured) {
        // NOTE: Выдаём указатель на данные уже вытащенной головы
        ptr = &(_buffer[idx]._mem);
        return true;
    }

    // NOTE: Если не вышли раньше значит ошибка
    return false;

    // // NOTE: Создаём сущности нового узла головы и копию текущей головы
    // stc_node_hdl new_top_hdl, top_hdl = _top.load();

    // // NOTE: После каждой неудачной попытки замещения
    // //       top_hdl будет обновляться текущей головой
    // //       средствами compare_exchange_weak(...)
    // do { if (top_hdl._node_idx == UINT32_MAX) [[unlikely]] return false;
    //     // NOTE: Новая голова должна иметь больше касаний чем старая
    //     new_top_hdl._tag
    //         = top_hdl._tag + 1;
    //     // NOTE: Новая голова должна иметь индекс буфера
    //     //       равный индексу следующего за головой узла
    //     new_top_hdl._node_idx
    //         = _buffer[top_hdl._node_idx]._next.load()._node_idx;
    //     // NOTE: Проводим замену только в том случае,
    //     //       если тег и индекс текущей головы остался неизменен,
    //     //       в голову пишется следующий за ней узел
    // } while (not _top.compare_exchange_weak(top_hdl, new_top_hdl));

    // // NOTE: Выдаём указатель на данные уже вытащенной головы
    // ptr = &(_buffer[top_hdl._node_idx]._mem);
    // return true;
}


#undef ATOMIC_BOUNDED_FREELIST_MEMBER
#endif // NUKES_ATOMIC_BOUNDED_FREELIST
