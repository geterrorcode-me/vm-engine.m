package com.vmeer.io;

import android.util.Log;

public class EngineLoader {
    static {
        try {
            // Memuat library yang kita buat di Android.mk
            System.loadLibrary("vmeer_engine");
        } catch (UnsatisfiedLinkError e) {
            Log.e("vMeer", "Gagal memuat vmeer_engine: " + e.getMessage());
        }
    }

    // Fungsi native yang terhubung ke jni/vm_engine.cpp
    public native boolean startEngine(String romPath);
}
