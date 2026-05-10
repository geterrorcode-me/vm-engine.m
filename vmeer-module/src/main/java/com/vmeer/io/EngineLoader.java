package com.vmeer.io;

import android.util.Log;
import android.view.Surface;

public class EngineLoader {
    static {
        try {
            // Urutan muat library
            System.loadLibrary("binder_vm");
            System.loadLibrary("art_vm");
            System.loadLibrary("romfs_vm");
            System.loadLibrary("bufferqueue_vm");
            System.loadLibrary("egl_bridge");
            System.loadLibrary("surface_vm");
            System.loadLibrary("vmeer_engine");
            Log.i("vMeer", "Semua modul engine berhasil dimuat.");
        } catch (UnsatisfiedLinkError e) {
            Log.e("vMeer", "Gagal memuat modul: " + e.getMessage());
        }
    }

    public native boolean startEngine(String romPath);
    public native void nativeUpdateDisplay(Surface surface);
}
