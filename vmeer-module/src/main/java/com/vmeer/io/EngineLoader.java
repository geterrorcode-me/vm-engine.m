package com.vmeer.io;

import android.util.Log;
import android.view.Surface;

public class EngineLoader {
    private static final String TAG = "vMeer_Loader";
    private static boolean isAllLibraryLoaded = false;

    static {
        try {
            Log.i(TAG, "Mulai memuat core engine vMeerOS...");
            
            // ====================================================================
            // PERBAIKAN KRUSIAL: Pangkas habis pemanggilan beruntun yang bikin crash.
            // Cukup muat binder_vm (untuk memuaskan dependensi awal) dan vmeer_engine.
            // Android akan otomatis memuat art_vm dan romfs_vm via target_link_libraries CMake.
            // ====================================================================
            System.loadLibrary("binder_vm");
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
