#include <malloc.h>

void* __real_malloc(size_t size);
void* __real_realloc(void* ptr, size_t size);
void __real_free(void* ptr);
void __real__free_r(struct _reent *r, void* ptr);
void* __real_memalign(size_t alignment, size_t size);

/**
 * @brief Linear alloc.
 * Always return NULL if safe linear alloc api is not initialized.
 * @param size (in) Memory size (in byte).
 * @return On success pointer, on failure NULL.
 * @note Thread safe
*/
void* Util_safe_linear_alloc(size_t size);

/**
 * @brief Linear align.
 * Always return NULL if safe linear alloc api is not initialized.
 * @param alignment (in) Alignment.
 * @param size (in) Memory size (in byte).
 * @return On success pointer, on failure NULL.
 * @note Thread safe
*/
void* Util_safe_linear_align(size_t alignment, size_t size);

/**
 * @brief Linear realloc.
 * Always return NULL if safe linear alloc api is not initialized.
 * @param pointer (in) Old pointer.
 * @param size (in) New memory size (in byte).
 * @return On success pointer, on failure NULL.
 * @note Thread safe
*/
void* Util_safe_linear_realloc(void* pointer, size_t size);

/**
 * @brief Free linear memory.
 * Do nothing if safe linear alloc api is not initialized.
 * @note Thread safe
*/
void Util_safe_linear_free(void* pointer);
