#include "safe_memory.h"
#include <3ds.h>

static bool util_safe_linear_alloc_init = false;
static LightLock util_safe_linear_alloc_mutex = 1;

static inline void init_safe_alloc() {
    if (util_safe_linear_alloc_init) {
        return;
    }
    LightLock_Init(&util_safe_linear_alloc_mutex);
    util_safe_linear_alloc_init = true;
}

void* __wrap_malloc(size_t size)
{
    void* ptr = NULL;
    //Alloc memory on linear ram if requested size is greater than 32KB to prevent slow down (linear alloc is slow).
    //If allocation failed, try different memory before giving up.
    if(size > 1024 * 32)
    {
        ptr = Util_safe_linear_alloc(size);
        if(!ptr)
            ptr = __real_malloc(size);
    }
    else
    {
        ptr = __real_malloc(size);
        if(!ptr)
            ptr = Util_safe_linear_alloc(size);
    }
    return ptr;
}

void* __wrap_realloc(void* ptr, size_t size)
{
    void* new_ptr[2] = { NULL, NULL, };

    //Alloc memory on linear ram if requested size is greater than 32KB
    //or previous pointer is allocated on linear ram to prevent slow down (linear alloc is slow).
    if(size > 1024 * 32 || (ptr >= (void*)OS_FCRAM_VADDR && ptr <= (void*)(OS_FCRAM_VADDR + OS_FCRAM_SIZE))
        || (ptr >= (void*)OS_OLD_FCRAM_VADDR && ptr <= (void*)(OS_OLD_FCRAM_VADDR + OS_OLD_FCRAM_SIZE)))
    {
        if(!ptr || (ptr >= (void*)OS_FCRAM_VADDR && ptr <= (void*)(OS_FCRAM_VADDR + OS_FCRAM_SIZE))
        || (ptr >= (void*)OS_OLD_FCRAM_VADDR && ptr <= (void*)(OS_OLD_FCRAM_VADDR + OS_OLD_FCRAM_SIZE)))
            return Util_safe_linear_realloc(ptr, size);
        else
        {
            //move onto linear ram
            new_ptr[0] = __real_realloc(ptr, size);
            if(new_ptr[0])
            {
                new_ptr[1] = Util_safe_linear_alloc(size);
                if(new_ptr[1])
                    memcpy(new_ptr[1], new_ptr[0], size);

                free(new_ptr[0]);
                return new_ptr[1];
            }
            else
                return new_ptr[0];
        }
    }
    else
        return __real_realloc(ptr, size);
}

void __wrap_free(void* ptr)
{
    if((ptr >= (void*)OS_FCRAM_VADDR && ptr <= (void*)(OS_FCRAM_VADDR + OS_FCRAM_SIZE))
    || (ptr >= (void*)OS_OLD_FCRAM_VADDR && ptr <= (void*)(OS_OLD_FCRAM_VADDR + OS_OLD_FCRAM_SIZE)))
        Util_safe_linear_free(ptr);
    else
        __real_free(ptr);
}

void __wrap__free_r(struct _reent *r, void* ptr)
{
    if((ptr >= (void*)OS_FCRAM_VADDR && ptr <= (void*)(OS_FCRAM_VADDR + OS_FCRAM_SIZE))
    || (ptr >= (void*)OS_OLD_FCRAM_VADDR && ptr <= (void*)(OS_OLD_FCRAM_VADDR + OS_OLD_FCRAM_SIZE)))
        Util_safe_linear_free(ptr);
    else
        __real__free_r(r, ptr);
}

void* __wrap_memalign(size_t alignment, size_t size)
{
    void* ptr = NULL;
    //Alloc memory on linear ram if requested size is greater than 32KB to prevent slow down (linear alloc is slow).
    //If allocation failed, try different memory before giving up.
    if(size > 1024 * 32)
    {
        ptr = Util_safe_linear_align(alignment, size);
        if(!ptr)
            ptr = __real_memalign(alignment, size);
    }
    else
    {
        ptr = __real_memalign(alignment, size);
        if(!ptr)
            ptr = Util_safe_linear_align(alignment, size);
    }
    return ptr;
}


void* Util_safe_linear_align(size_t alignment, size_t size)
{
    init_safe_alloc();

    LightLock_Lock(&util_safe_linear_alloc_mutex);
    void* pointer = linearMemAlign(size, alignment);
    LightLock_Unlock(&util_safe_linear_alloc_mutex);

    return pointer;
}

void* Util_safe_linear_realloc(void* pointer, size_t size)
{
    init_safe_alloc();

    if(size == 0)
    {
        Util_safe_linear_free(pointer);
        return pointer;
    }
    if(!pointer)
        return Util_safe_linear_alloc(size);

    void* new_ptr = Util_safe_linear_alloc(size);
    if(new_ptr)
    {
        LightLock_Lock(&util_safe_linear_alloc_mutex);
        u32 pointer_size = linearGetSize(pointer);
        LightLock_Unlock(&util_safe_linear_alloc_mutex);

        if(size > pointer_size)
            memcpy_asm((u8*)new_ptr, (u8*)pointer, pointer_size);
        else
            memcpy_asm((u8*)new_ptr, (u8*)pointer, size);

        Util_safe_linear_free(pointer);
    }
    return new_ptr;
}

void Util_safe_linear_free(void* pointer)
{
    init_safe_alloc();

    LightLock_Lock(&util_safe_linear_alloc_mutex);
    linearFree(pointer);
    LightLock_Unlock(&util_safe_linear_alloc_mutex);
}
