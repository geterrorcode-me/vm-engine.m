package com.vmeer.io;

import android.content.Context;
import android.util.Log;
import java.io.File;

public class EngineLoader {
    private static final String TAG = "vMeer_Bridge";

    public static native void init_art_hook();
    public static native void perform_mirror_injection(ClassLoader classLoader, String jarPath);

    /**
     * Membangunkan biner vmeerd dari folder internal aplikasi agar siap melayani VFS
     */
    private static void awakenDaemonProcess(Context context) {
        try {
            // Asumsi biner vmeerd hasil kompilasi ditaruh di direktori internal files/ bin aplikasi
            String internalBinDir = context.getFilesDir().getParent() + "/app_app_bin";
            String daemonPath = internalBinDir + "/vmeerd";
            
            File daemonFile = new File(daemonPath);
            if (daemonFile.exists()) {
                // Berikan akses eksekusi penuh jika terhapus/ter-reset otomatis oleh OS host
                daemonFile.setExecutable(true, false);
                
                Log.i(TAG, "vMeer Bootstrap: Membangunkan native daemon vmeerd...");
                Runtime.getRuntime().exec(new String[]{daemonPath});
                
                // Beri jeda 350ms agar abstract socket server selesai melakukan bind() di kernel
                Thread.sleep(350);
            } else {
                Log.w(TAG, "vMeer Bootstrap: File biner vmeerd tidak ditemukan di " + daemonPath);
            }
        } catch (Exception e) {
            Log.e(TAG, "vMeer Bootstrap: Gagal memicu eksekusi daemon: " + e.getMessage());
        }
    }

    public static void startFrameworkInjection(Context context, ClassLoader classLoader) {
        // ⚡ LANGKAH UTAMA KUNCI: Bangunkan daemon penata ruang nama berkas sebelum jar diakses
        awakenDaemonProcess(context);

        String vfsFrameworkDir = "/data/user/0/com.vmeer.io/app_app_bin/rootfs/system/framework/";
        
        String[] targetJars = {
            "core-oj.jar",
            "core-libart.jar",
            "ext.jar",
            "framework.jar",
            "services.jar"
        };

        Log.i(TAG, "vMeer OS: Memulai Lapisan 2 - Runtime Framework Injection...");

        // Jalankan bypass kebijakan hidden API internal Android 15
        try {
            init_art_hook();
        } catch (Throwable t) {
            Log.w(TAG, "Gagal eksekusi awal init_art_hook: " + t.getMessage());
        }

        int injectCount = 0;
        for (String jarName : targetJars) {
            File jarFile = new File(vfsFrameworkDir + jarName);
            
            if (jarFile.exists() && jarFile.isFile()) {
                Log.d(TAG, "Menyuntikkan biner komponen: " + jarName);
                try {
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
            Log.e(TAG, "CRITICAL ERROR: Jalur VFS '" + vfsFrameworkDir + "' kosong!");
            Log.e(TAG, "Pemeriksaan ulang: Cek apakah vmeerd diblokir oleh kebijakan latar belakang.");
        } else {
            Log.i(TAG, "vMeer OS: Lapisan Framework Berhasil Disatukan. Total: " + injectCount + " JAR.");
        }
    }
}
