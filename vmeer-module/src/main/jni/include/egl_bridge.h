#ifndef VMEER_EGL_BRIDGE_H
#define VMEER_EGL_BRIDGE_H

#include <EGL/egl.h>
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Memulai jembatan EGL grafis dengan mengikat native window host ke context GLES virtual.
 * @param window Pointer valid dari ANativeWindow hasil konversi Surface Java
 */
void start_egl_bridge(ANativeWindow* window);

/**
 * Menghentikan pipeline render thread grafis secara aman.
 */
void stop_egl_bridge();

/**
 * Fungsi pembantu pembacaan display environment milik vMeer
 */
EGLDisplay vmeer_eglGetDisplay(EGLNativeDisplayType display_id);

#ifdef __cplusplus
}
#endif

#endif // VMEER_EGL_BRIDGE_H
