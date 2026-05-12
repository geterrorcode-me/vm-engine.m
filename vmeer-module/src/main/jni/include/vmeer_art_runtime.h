#ifndef VMEER_ART_RUNTIME_H
#define VMEER_ART_RUNTIME_H

#include <jni.h>
#include <string>
#include <vector>

namespace vmeer {
namespace art {

class ARTRuntime {
public:
    static ARTRuntime& Get();
    
    // Fungsi utama untuk menjebol pagar Hidden API
    bool BypassHiddenApi();

    // Fungsi untuk menyuntikkan mirror.jar ke ClassLoader
    bool InjectMirrorJar(JNIEnv* env, jobject class_loader, const std::string& jar_path);

private:
    ARTRuntime() = default;
    bool m_bypassed = false;
};

} // namespace art
} // namespace vmeer

#endif
