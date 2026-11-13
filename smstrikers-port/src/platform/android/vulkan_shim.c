// Dawn opens libvulkan.so by name. This shim forwards its entry point to the
// selected custom driver or the system Vulkan loader.

#include <dlfcn.h>
#include <pthread.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <android/log.h>

#include <adrenotools/driver.h>

#define TAG "strikers"

typedef void (*PFN_vkVoidFunction)(void);
typedef PFN_vkVoidFunction (*PFN_vkGetInstanceProcAddr)(void* instance, const char* name);

// The Khronos loader that ships with the device, by absolute path. NOT by soname: this file is
// itself called libvulkan.so, and asking for the name would hand us back ourselves.
static const char* const kSystem[] = {
    "/system/lib64/libvulkan.so",
    "/system/lib/libvulkan.so",
};

static PFN_vkGetInstanceProcAddr s_gipa;
static pthread_once_t s_load_once = PTHREAD_ONCE_INIT;

PFN_vkVoidFunction vkGetInstanceProcAddr(void* instance, const char* name);

static const char* env(const char* name)
{
    const char* v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : NULL;
}

static PFN_vkGetInstanceProcAddr resolve_loader(void* handle)
{
    if (handle == NULL)
        return NULL;

    PFN_vkGetInstanceProcAddr entry =
        (PFN_vkGetInstanceProcAddr)dlsym(handle, "vkGetInstanceProcAddr");
    if (entry != NULL && entry != vkGetInstanceProcAddr)
        return entry; // Keep the library loaded for the process lifetime.

    dlclose(handle);
    return NULL;
}

static PFN_vkGetInstanceProcAddr open_system(void)
{
    for (size_t i = 0; i < sizeof kSystem / sizeof kSystem[0]; i++)
    {
        PFN_vkGetInstanceProcAddr entry =
            resolve_loader(dlopen(kSystem[i], RTLD_NOW | RTLD_LOCAL));
        if (entry != NULL)
        {
            __android_log_print(ANDROID_LOG_INFO, TAG, "[vk] System loader: %s", kSystem[i]);
            return entry;
        }
    }
    __android_log_print(ANDROID_LOG_ERROR, TAG, "[vk] No usable system Vulkan loader");
    return NULL;
}

// The hook libraries and the custom driver must be available as extracted files.
static PFN_vkGetInstanceProcAddr open_custom(void)
{
    const char* hookDir = env("STRIKERS_VK_HOOK_DIR");
    const char* driverDir = env("STRIKERS_VK_DRIVER_DIR");
    const char* driverName = env("STRIKERS_VK_DRIVER");

    if (driverDir == NULL || driverName == NULL)
        return NULL;
    if (hookDir == NULL)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "[vk] Custom driver %s selected without a hook directory",
                            driverName);
        return NULL;
    }

    void* handle = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM,
                                             NULL, hookDir, driverDir, driverName, NULL, NULL);
    PFN_vkGetInstanceProcAddr entry = resolve_loader(handle);
    if (entry == NULL)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "[vk] No usable Vulkan entry point for %s/%s; using system loader",
                            driverDir, driverName);
        return NULL;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "[vk] Custom driver: %s/%s", driverDir, driverName);
    return entry;
}

static void load(void)
{
    s_gipa = open_custom();
    if (s_gipa == NULL)
        s_gipa = open_system();
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetInstanceProcAddr(void* instance, const char* name)
{
    pthread_once(&s_load_once, load);
    if (s_gipa == NULL)
        return NULL;
    // Asked for itself: answer with ours rather than the driver's, so whoever keeps the pointer
    // keeps coming back through here.
    if (name != NULL && strcmp(name, "vkGetInstanceProcAddr") == 0)
        return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    return s_gipa(instance, name);
}
