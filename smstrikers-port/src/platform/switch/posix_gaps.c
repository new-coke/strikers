// POSIX calls that libnx lacks. Horizon has no users or file owners.

#include <errno.h>
#include <stddef.h>
#include <sys/iosupport.h>
#include <sys/types.h>
#include <unistd.h>

#include <switch.h>

// Weak, so a driver archive that exports its own takes precedence.
__attribute__((weak)) uid_t geteuid(void)
{
    return 0;
}

int fchown(int fd, uid_t owner, gid_t group)
{
    (void)fd;
    (void)owner;
    (void)group;
    return 0;
}

// libnx's private pthread_t, which its headers do not export.
struct __pthread_t
{
    Thread thr;
    void* rc;
};

void* __libnx_alloc(size_t size);
void __libnx_free(void* p);

typedef struct Detached
{
    struct __pthread_t* thread;
    struct Detached* next;
} Detached;

static Mutex s_detachedLock;
static Detached* s_detached;

// libnx has no pthread_detach; a detached thread is freed by a later detach once it has exited.
int __syscall_thread_detach(struct __pthread_t* thread)
{
    Detached* entry = __libnx_alloc(sizeof *entry);
    if (entry == NULL)
        return ENOMEM;

    mutexLock(&s_detachedLock);
    for (Detached** link = &s_detached; *link != NULL;)
    {
        Detached* done = *link;
        if (R_FAILED(svcWaitSynchronizationSingle(done->thread->thr.handle, 0)))
        {
            link = &done->next;
            continue;
        }
        *link = done->next;
        threadClose(&done->thread->thr);
        __libnx_free(done->thread);
        __libnx_free(done);
    }
    entry->thread = thread;
    entry->next = s_detached;
    s_detached = entry;
    mutexUnlock(&s_detachedLock);
    return 0;
}
