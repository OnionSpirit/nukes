#ifndef NUKES_ATOMIC_FREELIST
#define NUKES_ATOMIC_FREELIST

#include <atomic>
#include <cstddef>
#include <thread>

#include "nukes/details/constants.h"
#include "nukes/details/node_types.h"
#include "nukes/details/misc.h"
#include "atomic_lifo_pool.h"
#include "atomic_fifo_pool.h"


namespace nukes::pool {


template
<
    typename dataT,

    size_t bytesPerBucketV =
        details::constants::bucket_meta_data
      + sizeof(details::misc::meta_chunk<dataT, details::constants::bucket_meta_data>) * 64,

    template <typename, size_t> typename bufferT = atomic_lifo_pool,

    void* (mem_alloc) (size_t) = malloc,

    void  (mem_free)  (void*)  = free
>
class atomic_freelist {

    typedef details::misc::meta_chunk<dataT, details::constants::bucket_meta_data> bucket_chunk_t; ///< Bucket chunk with metadata (Source bucket ptr) support

    template <size_t sizeV>
    using bucket_buf_t = bufferT<bucket_chunk_t, sizeV>; ///< Bucket metatype with custom size

    static constexpr size_t bucket_control_data_size = sizeof(bucket_buf_t<1>); ///< Size of memory buffer structure with single element

    static_assert(std::same_as<bucket_buf_t<1>, atomic_fifo_pool<bucket_chunk_t, 1>>
                  or std::same_as<bucket_buf_t<1>, atomic_lifo_pool<bucket_chunk_t, 1>>,
                  "Only 'atomic_fifo_pool' and 'atomic_lifo_pool' pools allowed as 'bucket' basis"
                  );

    static_assert(details::constants::bucket_meta_data
                  + bucket_control_data_size
                  <= bytesPerBucketV,
                  "memory with bytes_per_bucketV size is not enough to emplace bucket for requested chunk_t there");

    static constexpr size_t bucket_chunk_count =
        (bytesPerBucketV
         - details::constants::bucket_meta_data
         - bucket_control_data_size
         ) / sizeof(bucket_chunk_t); ///< Count of chunks per bucket including list and chunk metadata,
                                     ///< bucket control data size, that can fit into bytes_per_bucketV memory chunk

    typedef bucket_buf_t<bucket_chunk_count> bucket_t;
    typedef bucket_t* bucket_ptr;

    struct alignas(8) bucket_node {
        std::atomic<bucket_node*> _next   { nullptr }; ///< Pointer to next bucket in buckets list (Bucket metadata)
        bucket_t                  _bucket { };         ///< Bucket instance first machine word
    };

private:

    bucket_node initial_bucket_node {}; ///< Static allocated first bucket node

    bucket_node* const _bucket_list_head { &initial_bucket_node }; ///< Bucket list first node

    std::atomic<bucket_node*> _bucket_list_tail { &initial_bucket_node }; ///< Bucket list last node

    std::atomic<bucket_node*> _recent_bucket { &initial_bucket_node }; ///< Bucket that was successfully used for allocation recently

    std::mutex _allocation_in_progress {}; ///< Allocation lock

    static void allocate_bucket_node(bucket_node*& node);

    static void deallocate_bucket_node(bucket_node*& node);

public:

    atomic_freelist(size_t buckets_count = 1);

    ~atomic_freelist();

    [[nodiscard]] bool sync(dataT*& ptr) noexcept;

    [[nodiscard]] bool capture(dataT*& ptr) noexcept;
};


// ================================ DEFINITIONS ================================

#define ATOMIC_FREELIST_MEMBER(member_type)                             \
    template <typename dataT, size_t bytes_per_bucketV,                 \
              template <typename, size_t> typename bufferT,             \
              void *(mem_alloc)(size_t), void(mem_free)(void *)>        \
    member_type nukes::pool::atomic_freelist <dataT,                    \
                                              bytes_per_bucketV, bufferT, mem_alloc, mem_free>::


ATOMIC_FREELIST_MEMBER()
atomic_freelist(size_t buckets_count) {

    // NOTE: Создаём указатели на текущий и предыдущий узлы
    bucket_node* prev = _bucket_list_head;
    bucket_node* curr = _bucket_list_head->_next.load(std::memory_order_consume);

    // NOTE: Пока счётчик не привысил запрашиваемое колличество бакетов
    //       аллоцируем новые поштучно, создавая связанный список
    for (int i = 0; i < buckets_count - 1; ++i) {
        allocate_bucket_node(curr);
        prev->_next.store(curr, std::memory_order_relaxed);
        prev = curr;
        curr = curr->_next;
    }

    // NOTE: Устанавливаем последний аллоцированный узел
    _bucket_list_tail.store(prev, std::memory_order_relaxed);
}


ATOMIC_FREELIST_MEMBER()
~atomic_freelist() {

    // NOTE: Записываем текущий узел как следующий от головного
    bucket_node* curr = _bucket_list_head->_next.load(std::memory_order_consume);
    // NOTE: Пока есть указатель выполняем очистку с переходом к следующему элементу
    while(curr) {
        auto next = curr->_next.load(std::memory_order_consume);
        mem_free(curr);
        curr = next;
    }
}


ATOMIC_FREELIST_MEMBER(void)
allocate_bucket_node(bucket_node*& node) {

    // NOTE: Выделяем блок памяти под бакет и сохраняем указатель
    //       на блок памяти в указатель узла списка бакетов
    node = reinterpret_cast<bucket_node*>(mem_alloc(bytes_per_bucketV));
    // NOTE: Обнуляем указатель на следующий бакет
    node->_next.store(nullptr, std::memory_order_relaxed);
    // NOTE: По адресу поля _data размещаем бакет
    new (&node->_bucket) bucket_t();
}


ATOMIC_FREELIST_MEMBER(void)
deallocate_bucket_node(bucket_node*& node) {

    uint8_t bucket [bytes_per_bucketV] = reinterpret_cast<uint8_t*>(node);
    mem_free(bucket);
    node = nullptr;
}


ATOMIC_FREELIST_MEMBER(bool)
sync(dataT*& ptr) noexcept {

    // NOTE: Преносим указатель на размер метаданных и интерпретируем этот как указатель на чанк
    bucket_chunk_t* mem = (bucket_chunk_t*)((uint8_t*)ptr - sizeof(details::constants::bucket_meta_data));

    // NOTE: Создаём указатель на бакет
    bucket_ptr sync_bucket { nullptr };
    // NOTE: Вычитываем из метаданных указатель на бакет
    mem->_meta_data.release(sync_bucket);
    // NOTE: Освобождаем данные в чанке
    const bool res = sync_bucket->sync(mem);

    ptr = nullptr;
    return res;
}


ATOMIC_FREELIST_MEMBER(bool)
capture(dataT*& ptr) noexcept {

    // NOTE: Считываем текущий бакет
    bucket_node* current_bucket = _recent_bucket.load(std::memory_order_consume);
    // NOTE: Сохраняем бакет с короторого начали поиск чанка
    bucket_node* start_bucket = current_bucket;
    // NOTE: Создаём указатель для сохранения чанка с метаданными
    bucket_chunk_t* mem;

    // NOTE: Пока выбранный бакет не выдаст память
    while (not current_bucket->_bucket.capture(mem)) {

        // NOTE: Переходим к следующему бакету
        current_bucket = current_bucket->_next.load(std::memory_order_consume);

        // NOTE: Если следующего бакета нет то откатываемся к базовому
        if (not current_bucket) current_bucket = &_bucket_list_head;

        // NOTE: Если указатель бакета вернулся к тому с которого начали,
        //       то пытаемся аллоцировать новый бакет
        if (current_bucket == start_bucket) {

            // NOTE: Пытаемся захватить флаг разрешения на аллокацию
            if (not _allocation_in_progress.try_lock()) {
                // NOTE: Если не удалось отпускаем поток
                std::this_thread::yield();
                // NOTE: После восстановления переключаемся на последний
                //       аллоцированный бакет, ожидая что аллокация завершилась
                current_bucket = _bucket_list_tail.load(std::memory_order_consume);
                // NOTE: Прерываем итерацию
                continue;
            }

            bucket_node* new_bucket { nullptr };
            allocate_bucket_node(new_bucket);
            // NOTE: Считываем крайний аллоцированый бакет и указываем
            //       новый аллоцированый как следующий
            _bucket_list_tail.load(std::memory_order_consume)->_next.store(new_bucket, std::memory_order_release);

            // NOTE: Указываем новый аллоцированый бакет как крайний алоцированный
            _bucket_list_tail.store(new_bucket, std::memory_order_release);
            // NOTE: Указываем новый аллоцированый бакет как выбраный
            current_bucket = new_bucket;

            // NOTE: Очищаем флаг аллокации бакета
            _allocation_in_progress.unlock();
        }
    }

    // NOTE: Сохраняем указатель на объект выбранного бакета в метаданных чанка
    mem->_meta_data = &current_bucket->_bucket;
    // NOTE: Записываем указатель выходных данных на область памяти чанка после метаданных
    ptr = &(mem->_mem);
    // NOTE: Указываем выбраный бакет как недавний
    _recent_bucket.store(current_bucket, std::memory_order_release);

    return true;
}


} // end namespace nukes


#undef ATOMIC_FREELIST_MEMBER
#endif // NUKES_ATOMIC_FREELIST
