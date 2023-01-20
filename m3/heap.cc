#include <m3/Compat.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>

extern void *_bss_end;
static uintptr_t heap_begin;
static uintptr_t heap_cur;
static uintptr_t heap_end;

EXTERN_C int __m3_heap_brk(uintptr_t) {
    // if this fails, the allocator will fall back to mmap
    return -ENOSYS;
}

EXTERN_C void __m3_heap_get_area(uintptr_t *begin, uintptr_t *end) {
    *begin = heap_begin;
    *end = heap_end;
}

EXTERN_C void __m3_heap_set_area(uintptr_t begin, uintptr_t end) {
    heap_begin = heap_cur = begin;
    heap_end = end;
}

EXTERN_C uintptr_t __m3_heap_get_end() {
    return heap_end;
}

EXTERN_C void __m3_heap_append(size_t pages) {
    heap_end += pages * PAGE_SIZE;
}

EXTERN_C void *__m3_heap_mmap(void *start, size_t len, int, int, int, off_t) {
    // ignore mappings at specific places (for guard page)
    if(start != nullptr)
        return MAP_FAILED;

    len = (len + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    if(heap_cur == 0) {
        heap_begin =
            m3::Math::round_up<uintptr_t>(reinterpret_cast<uintptr_t>(&_bss_end), PAGE_SIZE);
        if(m3::TileDesc(m3::env()->tile_desc).has_memory())
            heap_end = m3::TileDesc(m3::env()->tile_desc).stack_space().first;
        else
            heap_end = heap_begin + m3::env()->heap_size;
        heap_cur = heap_begin;
    }

    if(heap_cur + len > heap_end)
        return MAP_FAILED;

    void *res = reinterpret_cast<void *>(heap_cur);
    // musl expects the memory to be initialized; we pass the UNINIT flag for the heap mapping to
    // the pager and memset it here to ensure that we always initialize it exactly once, even if the
    // memory does not come from the pager.
    memset(res, 0, len);
    heap_cur += len;
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

EXTERN_C int __m3_heap_munmap(void *, size_t) {
    return 0;
}
