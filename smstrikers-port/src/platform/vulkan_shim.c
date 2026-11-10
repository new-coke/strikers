// A stand-in for the system's libvulkan.so, so the game can be pointed at a driver the system does
// not ship -- Turnip, Mesa's open Vulkan driver for Adreno.
//
// WHY A STAND-IN AND NOT A SETTING. Dawn opens Vulkan with dlopen("libvulkan.so") and takes exactly
// one symbol out of it, vkGetInstanceProcAddr; everything else it reaches through that. There is no
// hook, no environment variable and no argument for pointing it elsewhere, and the Dawn here is a
// prebuilt static library. So the choice is either to build Dawn from source with a patch, or to be
// the libvulkan.so it finds. This is the second.
//
// ONE SYMBOL IS THE WHOLE INTERFACE, and that is not a simplification: it was measured. The string
// "vkGetInstanceProcAddr" appears exactly once in libwebgpu_dawn.a, and "libvulkan.so" is the only
// Vulkan library name in it. Anything else this file exported would be for a caller that does not
// exist here, so it exports nothing else.
//
// HOW THE NAME RESOLVES. An Android app's linker namespace searches the app's own library directory
// before the system ones, so a libvulkan.so packaged in the APK is what dlopen("libvulkan.so")
// returns. Measured on a Galaxy S25+ running Android 16: the log line below appears and Dawn brings
// up the Adreno 830 through this file.

#include <dlfcn.h>
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

static void* s_handle;
static PFN_vkGetInstanceProcAddr s_gipa;
static int s_tried;

static const char* env(const char* name)
{
    const char* v = getenv(name);
    return (v != NULL && v[0] != '\0') ? v : NULL;
}

static void* open_system(void)
{
    size_t i;
    for (i = 0; i < sizeof kSystem / sizeof kSystem[0]; i++)
    {
        void* h = dlopen(kSystem[i], RTLD_NOW | RTLD_LOCAL);
        if (h != NULL)
        {
            __android_log_print(ANDROID_LOG_INFO, TAG, "[vk] driver do sistema: %s", kSystem[i]);
            return h;
        }
    }
    __android_log_print(ANDROID_LOG_ERROR, TAG, "[vk] nenhum libvulkan do sistema: %s", dlerror());
    return NULL;
}

/**
 * The driver the player chose, through libadrenotools.
 *
 * Three paths have to be right and none of them can be guessed from in here, so all three arrive in
 * the environment, put there by the activity before the library is loaded:
 *
 *   STRIKERS_VK_HOOK_DIR    getApplicationInfo().nativeLibraryDir, and nothing else. It is where
 *                           adrenotools finds the hooks it was built with, and it only holds real
 *                           files when the APK is packaged with useLegacyPackaging true.
 *   STRIKERS_VK_DRIVER_DIR  where the driver's .so is. It must NOT be on shared storage: dlopen
 *                           refuses a library any application could have edited, so the activity
 *                           copies it into the app's private files directory first.
 *   STRIKERS_VK_DRIVER      the driver's soname, e.g. libvulkan_freedreno.so.
 *
 * Returns NULL when there is nothing chosen or the load failed; the caller falls back to the
 * system, which is always better than not starting.
 */
static void* open_custom(void)
{
    const char* hookDir = env("STRIKERS_VK_HOOK_DIR");
    const char* driverDir = env("STRIKERS_VK_DRIVER_DIR");
    const char* driverName = env("STRIKERS_VK_DRIVER");
    void* h;

    if (driverDir == NULL || driverName == NULL)
        return NULL;
    if (hookDir == NULL)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "[vk] driver %s pedido mas STRIKERS_VK_HOOK_DIR nao foi definido",
                            driverName);
        return NULL;
    }

    h = adrenotools_open_libvulkan(RTLD_NOW | RTLD_LOCAL, ADRENOTOOLS_DRIVER_CUSTOM,
                                   NULL,        // tmpLibDir: memfd is used on API 29+
                                   hookDir,
                                   driverDir,
                                   driverName,
                                   NULL,        // fileRedirectDir
                                   NULL);       // userMappingHandle
    if (h == NULL)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "[vk] nao deu para carregar %s de %s; usando o do sistema",
                            driverName, driverDir);
        return NULL;
    }
    __android_log_print(ANDROID_LOG_INFO, TAG, "[vk] driver do jogador: %s/%s",
                        driverDir, driverName);
    return h;
}

static void load(void)
{
    if (s_tried)
        return;
    s_tried = 1;

    s_handle = open_custom();
    if (s_handle == NULL)
        s_handle = open_system();
    if (s_handle == NULL)
        return;

    s_gipa = (PFN_vkGetInstanceProcAddr)dlsym(s_handle, "vkGetInstanceProcAddr");
    if (s_gipa == NULL)
    {
        __android_log_print(ANDROID_LOG_ERROR, TAG,
                            "[vk] libvulkan sem vkGetInstanceProcAddr: %s", dlerror());
    }
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vkGetInstanceProcAddr(void* instance, const char* name)
{
    load();
    if (s_gipa == NULL)
        return NULL;
    // Asked for itself: answer with ours rather than the driver's, so whoever keeps the pointer
    // keeps coming back through here.
    if (name != NULL && strcmp(name, "vkGetInstanceProcAddr") == 0)
        return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    return s_gipa(instance, name);
}
