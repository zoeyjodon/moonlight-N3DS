#include <malloc.h>

void* __real_malloc(size_t size);
void* __real_realloc(void* ptr, size_t size);
void __real_free(void* ptr);
void __real__free_r(struct _reent *r, void* ptr);
void* __real_memalign(size_t alignment, size_t size);
