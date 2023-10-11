#include <m3/Compat.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

struct FreeMem {
    size_t size;
    FreeMem *next;
};

extern void *_bss_end;
static uintptr_t heap_begin;
static uintptr_t heap_end;
static FreeMem *free_list;

EXTERN_C int __m3_heap_brk(uintptr_t) {
    // if this fails, the allocator will fall back to mmap
    return -ENOSYS;
}

EXTERN_C void __m3_heap_get_area(uintptr_t *begin, uintptr_t *end) {
    *begin = heap_begin;
    *end = heap_end;
}

EXTERN_C void __m3_heap_set_area(uintptr_t begin, uintptr_t end) {
    heap_begin = begin;
    heap_end = end;
    free_list = reinterpret_cast<FreeMem *>(heap_begin);
    free_list->size = end - begin;
    free_list->next = nullptr;
}

EXTERN_C uintptr_t __m3_heap_get_end() {
    return heap_end;
}

EXTERN_C void __m3_heap_append(size_t pages) {
    FreeMem *cur = free_list;
    bool found = false;
    while(cur) {
        if(reinterpret_cast<uintptr_t>(cur) + cur->size == reinterpret_cast<uintptr_t>(heap_end)) {
            cur->size += pages * PAGE_SIZE;
            found = true;
            break;
        }
        cur = cur->next;
    }
    if(!found)
        abort();
    heap_end += pages * PAGE_SIZE;
}

EXTERN_C void *__m3_heap_mmap(void *start, size_t len, int, int, int, off_t) {
    // ignore mappings at specific places (for guard page)
    if(start != nullptr)
        return MAP_FAILED;

    len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if(heap_begin == 0) {
        uintptr_t end, begin = m3::Math::round_up<uintptr_t>(reinterpret_cast<uintptr_t>(&_bss_end),
                                                             PAGE_SIZE);
        if(m3::TileDesc(m3::env()->tile_desc).has_memory())
            end = m3::TileDesc(m3::env()->tile_desc).stack_space().first;
        else
            end = begin + m3::env()->heap_size;
        __m3_heap_set_area(begin, end);
    }

    void *res = nullptr;
    FreeMem *cur = free_list;
    while(cur) {
        if(cur->size >= len) {
            if(cur->size == len)
                free_list = cur->next;
            else {
                free_list = reinterpret_cast<FreeMem *>(reinterpret_cast<uintptr_t>(cur) + len);
                free_list->size = cur->size - len;
                free_list->next = cur->next;
            }
            res = cur;
            break;
        }
        cur = cur->next;
    }

    if(!res)
        return MAP_FAILED;

    // musl expects the memory to be initialized; we pass the UNINIT flag for the heap mapping to
    // the pager and memset it here to ensure that we always initialize it exactly once, even if the
    // memory does not come from the pager.
    memset(res, 0, len);
    return res;
}

EXTERN_C void *__m3_heap_mremap(void *, size_t, size_t, int, ...) {
    return MAP_FAILED;
}

EXTERN_C int __m3_heap_mprotect(void *, size_t, int) {
    return 0;
}

EXTERN_C int __m3_heap_madvise(void *, size_t, int) {
    return 0;
}

EXTERN_C int __m3_heap_munmap(void *ptr, size_t size) {
    FreeMem *f = reinterpret_cast<FreeMem *>(ptr);
    f->size = size;
    f->next = free_list;
    free_list = f;
    return 0;
}
