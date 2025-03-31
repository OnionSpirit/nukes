#ifndef NUKES_ATOMIC_RINGBUF
#define NUKES_ATOMIC_RINGBUF

#include <atomic>
#include <cstdint>
#include <sys/types.h>
#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"



namespace nukes::bounded {

template <typename dataT, details::constants::hword lenV = 1024>
struct atomic_ringbuf {

protected:

    struct control_indexes {
        details::constants::hword _head_idx { details::constants::hword_max_v };
        details::constants::hword _tail_idx { details::constants::hword_max_v };
    };

    typedef std::atomic<bool> meta_info;
    typedef std::atomic<control_indexes> control_indexes_t;
    typedef details::misc::meta_chunk<dataT, sizeof(meta_info)> data_chunk_t;
    typedef details::misc::meta_data<lenV * sizeof(data_chunk_t)> storage_t;

    control_indexes_t                _indexes {};           // NOTE: Квази-указатель головы и хвоста
    data_chunk_t*                    _buffer  { nullptr };  // NOTE: Буфер хранения памяти
    const details::constants::hword  _len     { lenV };
    storage_t                        _storage {};

public:

    atomic_ringbuf() noexcept
        requires ( lenV != details::constants::runtime_discover );

    atomic_ringbuf(details::constants::hword) noexcept
        requires ( lenV == details::constants::runtime_discover );

    ~atomic_ringbuf() noexcept =default;

    // NOTE: Освобождение чанка по индексу
    [[nodiscard]] bool push(details::misc::fn_forward_t<dataT> data) noexcept;

    // NOTE: Захват чанка по индексу
    [[nodiscard]] bool pop(dataT& data) noexcept;
};


} // end namespace nukes


// ================================ DEFINITIONS ================================

#define ATOMIC_RINGBUF_MEMBER(member_type)                              \
    template <typename dataT, nukes::details::constants::hword lenV>    \
    member_type nukes::bounded::atomic_ringbuf<dataT, lenV>::


ATOMIC_RINGBUF_MEMBER()
atomic_ringbuf() noexcept
requires ( lenV != nukes::details::constants::runtime_discover ) {

    // NOTE: При статическом определении размера ссылаем указатель буфера на начало хранилища,
    //       их размер соответствует запрошенному через шаблонный параметр
    _buffer = new (&_storage.template release<data_chunk_t>()) data_chunk_t[_len];
    _indexes.store( control_indexes{}, std::memory_order_relaxed);
}

ATOMIC_RINGBUF_MEMBER()
atomic_ringbuf(nukes::details::constants::hword l) noexcept
requires(lenV == nukes::details::constants::runtime_discover)
: _len(l) {

    // NOTE: При динамическом определении размера, аллоцируем на куче нужный размер,
    //       сохраняем указатель в хранилище и записываем его в буфер
    _buffer = (_storage = new data_chunk_t[_len]).template release<data_chunk_t*>();
    _indexes.store( control_indexes{}, std::memory_order_relaxed);
}


ATOMIC_RINGBUF_MEMBER(bool)
push(details::misc::fn_forward_t<dataT> data) noexcept {

    // NOTE: Создаём сущности новой индексной пары и копию текущей индексной пары
    control_indexes new_indexes, indexes = _indexes.load();

    // NOTE: Индекс буфера для записи данных
    details::constants::hword store_idx {};

    do {
        // NOTE: Устанавливаем индекс новой пары
        new_indexes = indexes;

        // NOTE: Т.к. максимальное значение считается пустотой необходимо просчитать инкремент
        const ushort index_offset {
            indexes._tail_idx == details::constants::hword_max_v - 1 ? 2 : 1
        };

        // NOTE: Приращаем индекс хвоста и сохраняем индекс для записи
        store_idx = new_indexes._tail_idx += index_offset;

        // NOTE: Перезапускаем итерацию, если хвостовой элемент ещё не очищен в буфере или записан другом потоком
        if (((meta_info)_buffer[new_indexes._head_idx]._meta_data).load(std::memory_order_acquire))
            continue;

        // NOTE: Выходим если буфер полон
        if (new_indexes._tail_idx == new_indexes._head_idx) return false;

        // NOTE: Если головной индекс пуст приравниваем его к хвостовому
        if (new_indexes._head_idx == details::constants::hword_max_v)
            new_indexes._head_idx = new_indexes._tail_idx;

        // NOTE: Пробуем заменить индексную пару
    } while (not _indexes.compare_exchange_weak(indexes, new_indexes,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));

    // NOTE: Сохраняем данные в буфер
    _buffer[store_idx]._mem = std::forward<dataT>(data);

    // NOTE: Взводим флаг что данные записаны в буфер
    ((meta_info)_buffer[store_idx]._meta_data).store(true);

    return true;
}


ATOMIC_RINGBUF_MEMBER(bool)
pop(dataT& data) noexcept {

    // NOTE: Создаём сущности новой индексной пары и копию текущей индексной пары
    control_indexes new_indexes, indexes = _indexes.load();

    // NOTE: Индекс буфера для записи данных
    details::constants::hword store_idx {};

    do {
        // NOTE: Устанавливаем индекс новой пары
        new_indexes = indexes;

        // NOTE: Выходим если буфер пуст
        if (new_indexes._head_idx == details::constants::hword_min_v)
            return false;

        // NOTE: Перезапускаем итерацию, если головной элемент ещё не записался в буфер
        if (((meta_info)_buffer[new_indexes._head_idx]._meta_data).load(std::memory_order_acquire))
            continue;

        // NOTE: Т.к. максимальное значение считается пустотой необходимо просчитать дикремент
        const ushort index_offset {
            indexes._head_idx == details::constants::hword_min_v ? 2 : 1
        };

        // NOTE: Сохраняем индекс для очистки
        store_idx = new_indexes._head_idx;

        // NOTE: Понижаем индекс головы
        new_indexes._head_idx -= index_offset;

        // NOTE: Задаем максимальное значение индексам как пустое,
        //       т.к. они сравнялись, а значит буфер опустошился
        if (new_indexes._head_idx == new_indexes._tail_idx)
            new_indexes._head_idx = new_indexes._tail_idx = details::constants::hword_max_v;

        // NOTE: Пробуем заменить индексную пару
    } while (not _indexes.compare_exchange_weak(indexes, new_indexes,
                                                std::memory_order_release,
                                                std::memory_order_relaxed));

    // NOTE: Читаем данные из буфера
    data = std::forward<dataT>(_buffer[store_idx]._mem);

    // NOTE: Взводим флаг что данные в буфере очищены
    ((meta_info)_buffer[store_idx]._meta_data).store(false);

    return true;

}

#undef ATOMIC_RINGBUF_MEMBER
#endif // NUKES_ATOMIC_RINGBUF
