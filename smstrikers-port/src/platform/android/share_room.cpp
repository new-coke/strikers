#if defined(__ANDROID__)

#include <SDL3/SDL_system.h>
#include <jni.h>

void PortAndroidShareRoomAddress(const char* address)
{
    auto* env = static_cast<JNIEnv*>(SDL_GetAndroidJNIEnv());
    if (env == nullptr)
        return;

    auto activity = static_cast<jobject>(SDL_GetAndroidActivity());
    if (activity == nullptr)
        return;

    jclass type = env->GetObjectClass(activity);
    jmethodID method = type == nullptr ? nullptr
        : env->GetMethodID(type, "shareRoomAddress", "(Ljava/lang/String;)V");
    if (method != nullptr)
    {
        jstring text = env->NewStringUTF(address);
        if (text != nullptr)
        {
            env->CallVoidMethod(activity, method, text);
            env->DeleteLocalRef(text);
        }
    }
    if (env->ExceptionCheck())
        env->ExceptionClear();
    env->DeleteLocalRef(type);
    env->DeleteLocalRef(activity);
}

#endif
