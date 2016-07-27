#include <atomic>
#include "MemAllocator.h"


#define THROW_BAD_ALLOC std::cerr << "out of memory" << std::endl; exit(-1);

const int defaultNodeNum = 20;
const int InitPoolSize = 2048;

class AllocPrime {
public:
    static void* allocate(size_t n);
    static void* reallocate(void* p, size_t new_sz);
    static void  deallocate(void *p);
    static void (*set_oom_malloc_handler(void (*f)())) ();

private:
    static void* oom_malloc(size_t n);
    static void* oom_realloc(void*p, size_t n);
    static void (*malloc_oom_handler) ();
    static std::atomic_bool malloc_oom_handlerRD;
};

class AllocImpl {
public:
    enum {
        ALIGN = 8,
        MAX_BYTES = 256,
        NFREELISTS = MAX_BYTES / ALIGN
    };

public:
    static AllocImpl& Instance();
    void* allocate(size_t n);
    void  deallocate(void* p, size_t n);
    void* reallocate(void*p, size_t old_sz, size_t new_sz);

private:
    AllocImpl();
    AllocImpl& operator=(AllocImpl&) {return *this;}
    AllocImpl(AllocImpl&) {}

    size_t ROUND_UP(size_t bytes) {
        return (((bytes) + ALIGN -1) & ~ (ALIGN - 1));
    }

    size_t FreeListIndex(size_t bytes) {
        return (((bytes) + ALIGN - 1) / ALIGN - 1);
    }

    char* chunk_alloc(size_t size, int &nobjs);
    void* refill(size_t n);



private:
    union obj {
        union obj* free_list_link;
        char  client[1];
    };

private:
    obj* volatile     free_list[NFREELISTS];
    std::atomic_bool  free_listRD[NFREELISTS];

    char              *start_free;
    char              *end_free;
    size_t            heap_size;
    std::atomic_bool  poolRD;
    std::atomic_bool  chunk_allocRD;
};


///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
void (*AllocPrime::malloc_oom_handler)() = 0;
std::atomic_bool AllocPrime::malloc_oom_handlerRD(true);

void *AllocPrime::allocate(size_t n)
{
    void* result = malloc(n);
    if(0 == result) result = oom_malloc(n);

    return result;
}

void *AllocPrime::reallocate(void *p, size_t new_sz)
{
    void* result = realloc(p, new_sz);
    if(0 == result) oom_realloc(p, new_sz);

    return result;
}

void AllocPrime::deallocate(void *p)
{
    free(p);
}

void (* AllocPrime::set_oom_malloc_handler(void (*f)())) ()
{
    void (*old)() = malloc_oom_handler;
    while(malloc_oom_handlerRD == false) usleep(1);
    malloc_oom_handlerRD = false;
    malloc_oom_handler = f;
    malloc_oom_handlerRD = true;
    return old;
}

void *AllocPrime::oom_malloc(size_t n)
{
    void (*my_malloc_handler)() = 0;
    void *result = 0;

    while(true) {
        my_malloc_handler = malloc_oom_handler;
        if(0 == my_malloc_handler) {THROW_BAD_ALLOC}
        (*my_malloc_handler)();

        result = malloc(n);
        if(result) return result;
    }
}

void *AllocPrime::oom_realloc(void *p, size_t n)
{
    void (*my_malloc_handler)() = 0;
    void *result = 0;

    while(true) {
        my_malloc_handler = malloc_oom_handler;
        if(0 == my_malloc_handler) {THROW_BAD_ALLOC}
        (*my_malloc_handler)();

        result = realloc(p, n);
        if(result) return result;
    }
}

///////////////////////////////////////////////////////
///////////////////////////////////////////////////////
///////////////////////////////////////////////////////

AllocImpl& AllocImpl::Instance() {
    static AllocImpl theOneAndOnly;
    return theOneAndOnly;
}

void *AllocImpl::allocate(size_t n)
{
    if(n > (size_t) MAX_BYTES) {
        return AllocPrime::allocate(n);
    }

    int idx = FreeListIndex(n);
    while(free_listRD[idx] == false) std::this_thread::yield();
    free_listRD[idx] = false;
    obj* volatile *my_free_list = free_list + idx;

REFILL:
    obj* result = *my_free_list;

    if(result == 0) {
        void *r = refill(ROUND_UP(n));
        if(r == 0) {
            usleep(1);
            goto REFILL;
        }
        free_listRD[idx] = true;
        return r;
    }

    *my_free_list = result->free_list_link;
    free_listRD[idx] = true;
    return result;
}

void AllocImpl::deallocate(void *p, size_t n)
{
    if(n > (size_t)MAX_BYTES) {
        AllocPrime::deallocate(p);
        return;
    }

    obj *q = (obj*)p;
    int idx = FreeListIndex(n);
    while(free_listRD[idx] == false) std::this_thread::yield();
    free_listRD[idx] = false;
    obj* volatile *my_free_list = free_list + idx;

    q->free_list_link = *my_free_list;
    *my_free_list = q;
    free_listRD[idx] = true;
}

void *AllocImpl::reallocate(void *p, size_t old_sz, size_t new_sz)
{
    deallocate(p, old_sz);
    p = allocate(new_sz);
    return p;
}

AllocImpl::AllocImpl()
{
    for(int i = 0; i < NFREELISTS; ++i) {
        free_list[i] = 0;
        free_listRD[i] = true;
    }

    heap_size = InitPoolSize;
    start_free = (char*)malloc(InitPoolSize);
    end_free = heap_size + start_free;

    poolRD = true;
    chunk_allocRD = true;
}


void* AllocImpl::refill(size_t n)
{
    int nobjs = defaultNodeNum;
    char *chunck = chunk_alloc(n, nobjs);
    if(chunck == 0) return 0;
    obj* volatile *my_free_list = 0;
    obj* result = 0;
    obj *current_obj(0), *next_obj(0);

    if(1 == nobjs) return chunck;

    int idx = FreeListIndex(n);

    my_free_list = free_list + idx;
    result = (obj*)chunck;

    *my_free_list = next_obj = (obj*)(chunck + n);

    for(int i = 1;; ++i) {
        current_obj = next_obj;
        next_obj = (obj*)((char*)next_obj + n);
        if(nobjs - 1 == i) {
            current_obj->free_list_link = 0;
            break;
        }
        else {
            current_obj->free_list_link = next_obj;
        }
    }

    return result;
}


char* AllocImpl::chunk_alloc(size_t size, int &nobjs)
{
    char *result = 0;
    size_t total_bytes = size * nobjs;
    while(poolRD == false) std::this_thread::yield();
    poolRD = false;
    size_t bytes_left = end_free - start_free;
    if(bytes_left > total_bytes) {
        result = start_free;
        start_free += total_bytes;
        poolRD = true;
        return result;
    }

    else if(bytes_left >= size) {
        nobjs = bytes_left / size;
        total_bytes = size * nobjs;
        result = start_free;
        start_free += total_bytes;
        poolRD = true;
        return result;
    }

    else {
        if(chunk_allocRD == false) {
            poolRD = true;
            return 0;
        }
        chunk_allocRD = false;

        size_t bytes_to_get = 2 * total_bytes + ROUND_UP(heap_size >> 4);
        if(bytes_left > 0) {
            obj* volatile *my_free_list = free_list + FreeListIndex(bytes_left);
            ((obj*)start_free)->free_list_link = *my_free_list;
            *my_free_list = (obj*)start_free;
        }

        start_free = (char*)malloc(bytes_to_get);
        if(0 == start_free) {
            end_free = 0;
            start_free = (char*)AllocPrime::allocate(bytes_to_get);
        }
        heap_size += bytes_to_get;
        end_free  = start_free + bytes_to_get;
        chunk_allocRD = true;
        poolRD = true;
        return (chunk_alloc(size, nobjs));
    }
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void *Alloc::allocate(size_t n)
{
    return AllocImpl::Instance().allocate(n);
}

void Alloc::deallocate(void *p, size_t n)
{
    return AllocImpl::Instance().deallocate(p, n);
}

void *Alloc::reallocate(void *p, size_t old_sz, size_t new_sz)
{
    return AllocImpl::Instance().reallocate(p, old_sz, new_sz);
}

void (* Alloc::set_oom_malloc_handler(void (*f)())) ()
{
    return AllocPrime::set_oom_malloc_handler(f);
}




