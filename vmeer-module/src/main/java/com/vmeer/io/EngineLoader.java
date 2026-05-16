package com.vmeer.io;

import android.util.Log;
import android.view.Surface;

public class EngineLoader {
    private static final String TAG = "vMeer_Loader";
    private static boolean isAllLibraryLoaded = false;

    static {
        try {
            Log.i(TAG, "Mulai memuat sub-modul virtualisasi secara sekuensial...");
            
            // 1. FONDASI SUBSYSTEM: Muat runtime Android virtual & Binder IPC
            System.loadLibrary("binder_vm");
            System.loadLibrary("art_vm");
            
            // 2. VFS LAYER: Muat sistem file SquashFS (readonly.bin reader)
            System.loadLibrary("romfs_vm");
            
            // 3. GRAPHICS LAYER: Muat buffer layar dan jembatan EGL SwiftShader
            System.loadLibrary("bufferqueue_vm");
            System.loadLibrary("egl_bridge");
            System.loadLibrary("surface_vm");
            
            // 4. MAIN CORE JNI: Satukan semua subsistem ke dalam JNI Engine Utama
            System.loadLibrary("vmeer_engine");
            
            isAllLibraryLoaded = true;
            Log.i(TAG, "SUCCESS: Semua modul native vMeer Engine berhasil dikunci ke memori.");
        } catch (UnsatisfiedLinkError e) {
            isAllLibraryLoaded = false;
            Log.e(TAG, "FATAL: Gagal memuat rangkaian library native: " + e.getMessage());
        }
    }

    /**
     * Memeriksa apakah seluruh library .so berhasil dimuat tanpa korup
     */
    public static boolean isEngineReadyToLoad() {
        return isAllLibraryLoaded;
    }

    // --- NATIVE INTERFACE (C++ Bridges) ---

    /**
     * Memulai booting biner init di dalam kontainer menggunakan TIOSeccomp
     */
    public native boolean startEngine(String romPath);

    /**
     * Mengirimkan surface display dari MainActivity Java ke SwiftShader C++ via Shared Memory
     */
    public native void nativeUpdateDisplay(Surface surface);
}
