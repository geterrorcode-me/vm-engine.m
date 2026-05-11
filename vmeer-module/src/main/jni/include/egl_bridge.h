#ifndef VMEER_EGL_BRIDGE_H
#define VMEER_EGL_BRIDGE_H

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Memulai proses hooking pada libEGL.so.
 * Menargetkan eglSwapBuffers dan eglCreateWindowSurface.
 */
void start_egl_bridge();

/**
 * Fungsi utilitas untuk mendapatkan informasi display saat ini
 */
EGLDisplay get_active_vmeer_display();

#ifdef __cplusplus
}
#endif

#endif // VMEER_EGL_BRIDGE_H
