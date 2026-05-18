package com.vmeer.io;

import android.content.Context;
import android.util.Log;
import java.io.File;

public class EngineLoader {
    private static final String TAG = "vMeer_Bridge";

    // Deklarasi fungsi native JNI dari art_vm.cpp
    public static native void init_art_hook();
    public static native void perform_mirror_injection(ClassLoader classLoader, String jarPath);

    /**
     * Memulai proses injeksi berantai untuk menyatukan Framework OS Tamu
     * ke dalam ClassLoader aplikasi host.
     */
    public static void startFrameworkInjection(Context context, ClassLoader classLoader) {
        // Lokasi absolut titik mount VFS hasil ekstraksi/mount readonly.bin
        String vfsFrameworkDir = "/data/user/0/com.vmeer.io/app_app_bin/rootfs/system/framework/";
        
        // Urutan Boot Jars krusial untuk kestabilan Android Runtime (ART) Guest OS
        String[] targetJars = {
            "core-oj.jar",
            "core-libart.jar",
            "ext.jar",
            "framework.jar",
            "services.jar"
        };

        Log.i(TAG, "vMeer OS: Memulai Lapisan 2 - Runtime Framework Injection...");

        int injectCount = 0;
        for (String jarName : targetJars) {
            File jarFile = new File(vfsFrameworkDir + jarName);
            
            if (jarFile.exists() && jarFile.isFile()) {
                Log.d(TAG, "Menyuntikkan biner komponen: " + jarName);
                try {
                    // Eksekusi injeksi ke native layer art_vm
                    perform_mirror_injection(classLoader, jarFile.getAbsolutePath());
                    injectCount++;
                } catch (Exception e) {
                    Log.e(TAG, "Gagal menginjeksi komponen " + jarName + ": " + e.getMessage());
                }
            } else {
                Log.w(TAG, "Peringatan: Komponen '" + jarName + "' tidak ditemukan di mount VFS. Melewati...");
            }
        }

        if (injectCount == 0) {
            Log.e(TAG, "CRITICAL ERROR: Tidak ada satu pun berkas framework .jar yang berhasil diinjeksi!");
            Log.e(TAG, "Pastikan vmeerd telah berhasil mem-mount readonly.bin ke: " + vfsFrameworkDir);
        } else {
            Log.i(TAG, "vMeer OS: Lapisan Framework Berhasil Disatukan. Total: " + injectCount + " JAR.");
        }
    }
}
