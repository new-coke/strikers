typedef struct DestructorChain
{
    struct DestructorChain* next;
    void* destructor;
    void* object;
} DestructorChain;

DestructorChain* __global_destructor_chain;

/**
 * Offset/Address/Size: 0x0 | 0x8023A154 | size: 0x48
 */
void __destroy_global_chain(void)
{
    DestructorChain* gdc;

    while ((gdc = __global_destructor_chain) != 0)
    {
        __global_destructor_chain = gdc->next;
        ((void (*)(void*, short))gdc->destructor)(gdc->object, -1);
    }
}

/**
 * Offset/Address/Size: 0x48 | 0x8023A19C | size: 0x18
 */
void* __register_global_object(void* object, void* destructor, void* regmem)
{
    ((DestructorChain*)regmem)->next = __global_destructor_chain;
    ((DestructorChain*)regmem)->destructor = destructor;
    ((DestructorChain*)regmem)->object = object;
    __global_destructor_chain = (DestructorChain*)regmem;

    return object;
}

__declspec(section ".dtors") static void* const __destroy_global_chain_reference = __destroy_global_chain;
