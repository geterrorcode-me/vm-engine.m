package com.vmeer.io;

import android.content.Context;
import android.util.Log;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

public class EngineLoader {
    private static final String TAG = "vMeer_Bridge";

    // Fungsi Native JNI
    public static native void init_art_hook();
    public static native void perform_mirror_injection(ClassLoader classLoader, String jarPath);

    /**
     * Memastikan biner vmeerd ter-deploy dari assets ke internal storage privat aplikasi.
     */
    private static void deployDaemonFromAssets(Context context, File targetDaemonFile) {
        // Buat folder induk (app_bin) jika belum tercipta
        File parentDir = targetDaemonFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            parentDir.mkdirs();
        }

        // Ekstrak hanya jika berkas belum ada di internal storage
        if (!targetDaemonFile.exists()) {
            Log.i(TAG, "vMeer Bootstrap: Menggandakan biner vmeerd dari assets ke storage privat...");
            try (InputStream in = context.getAssets().open("vmeer_bin/vmeerd");
                 OutputStream out = new FileOutputStream(targetDaemonFile)) {
                
                byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
                out.flush();
                Log.i(TAG, "vMeer Bootstrap: Deployment vmeerd sukses.");
                
            } catch (IOException e) {
                Log.e(TAG, "vMeer Bootstrap: CRITICAL! Gagal mengekstrak biner vmeerd: " + e.getMessage());
                return;
            }
        }

        // 🔥 PROTEKSI EKSEKUSI: Berikan hak akses Executable penuh (rwxr-xr-x)
        if (targetDaemonFile.exists()) {
            boolean isExecSet = targetDaemonFile.setExecutable(true, false);
            Log.d(TAG, "vMeer Safety: Mengunci izin eksekusi vmeerd -> Status Executable: " + isExecSet);
        }
    }

    /**
     * Mengekstrak berkas .jar pendukung dari assets ke dalam struktur folder VFS rootfs
     */
    private static void extractFrameworkFromAssets(Context context, String targetDirPath) {
        File vfsFrameworkDir = new File(targetDirPath);
        
        if (!vfsFrameworkDir.exists()) {
            boolean created = vfsFrameworkDir.mkdirs();
            Log.d(TAG, "vMeer Assets: Membuat struktur direktori VFS -> " + created);
        }

        String[] jarFiles = {
            "core-oj.jar", 
            "core-libart.jar", 
            "ext.jar", 
            "framework.jar", 
            "services.jar"
        };

        for (String jar : jarFiles) {
            File targetFile = new File(vfsFrameworkDir, jar);
            
            if (!targetFile.exists()) {
                try (InputStream in = context.getAssets().open("vmeer_framework/" + jar);
                     OutputStream out = new FileOutputStream(targetFile)) {
                    
                    byte[] buffer = new byte[8192];
                    int read;
                    while ((read = in.read(buffer)) != -1) {
                        out.write(buffer, 0, read);
                    }
                    out.flush();
                    Log.i(TAG, "vMeer Assets: Sukses menyalin " + jar + " ke VFS Rootfs.");
                    
                } catch (IOException e) {
                    Log.e(TAG, "vMeer Assets: Gagal mengekstrak komponen framework: " + jar + " -> " + e.getMessage());
                    continue;
                }
            }

            // 🔥 TAHAP BYPASS ANDROID 15: Cabut akses writable agar ART bersedia mengurai DEX
            if (targetFile.exists()) {
                boolean setReadOnly = targetFile.setReadOnly();
                boolean revokeWrite = targetFile.setWritable(false, false); 
                
                Log.d(TAG, "vMeer Safety: Mengunci berkas " + jar + 
                      " -> Status ReadOnly: " + setReadOnly + ", Revoke Writable: " + revokeWrite);
            }
        }
    }

    /**
     * Membangunkan biner vmeerd dari folder internal data aplikasi secara dinamis
     */
    private static String awakenDaemonProcess(Context context) {
        try {
            String baseDataDir = context.getFilesDir().getParent();
            
            // Tentukan jalur prioritas utama di /app_bin/vmeerd
            File daemonFile = new File(baseDataDir + "/app_bin/vmeerd");
            
            // Jalankan sub-rutin deployment untuk memastikan eksistensi fisik berkas
            deployDaemonFromAssets(context, daemonFile);
            
            if (daemonFile.exists()) {
                Log.i(TAG, "vMeer Bootstrap: Membangunkan native daemon vmeerd dari -> " + daemonFile.getAbsolutePath());
                Runtime.getRuntime().exec(new String[]{daemonFile.getAbsolutePath()});
                
                // Beri jeda 350ms agar abstract socket server selesai melakukan bind() di kernel
                Thread.sleep(350);
                
                return daemonFile.getParentFile().getName(); // Mengembalikan "app_bin"
            } else {
                Log.w(TAG, "vMeer Bootstrap: File biner vmeerd tetap tidak ditemukan setelah fase force-deploy.");
            }
        } catch (Exception e) {
            Log.e(TAG, "vMeer Bootstrap: Gagal memicu eksekusi daemon: " + e.getMessage());
        }
        return "app_bin";
    }

    /**
     * Titik entri utama Lapisan 2 untuk menyatukan struktur Java Runtime Framework kontainer
     */
    public static void startFrameworkInjection(Context context, ClassLoader classLoader) {
        // ⚡ LANGKAH PRE-BOOT 1: Deploy & Bangunkan daemon vmeerd
        String activeBinFolder = awakenDaemonProcess(context);
        
        // Sesuaikan target lokasi rootfs system framework berdasarkan folder aktif
        String vfsFrameworkDir = context.getFilesDir().getParent() + "/" + activeBinFolder + "/rootfs/system/framework/";

        // ⚡ LANGKAH PRE-BOOT 2: Ekstrak berkas dan amankan tanda tangan Read-Only sebelum diendus ART
        Log.i(TAG, "vMeer OS: Memeriksa kesiapan berkas fisik Java Framework di VFS -> " + vfsFrameworkDir);
        extractFrameworkFromAssets(context, vfsFrameworkDir);
        
        String[] targetJars = {
            "core-oj.jar",
            "core-libart.jar",
            "ext.jar",
            "framework.jar",
            "services.jar"
        };

        Log.i(TAG, "vMeer OS: Memulai Lapisan 2 - Runtime Framework Injection...");

        // Jalankan bypass kebijakan hidden API internal Android 15 jika library .so telah siap
        try {
            init_art_hook();
        } catch (Throwable t) {
            Log.w(TAG, "Sistem menunda eksekusi awal init_art_hook (Library native belum dimuat): " + t.getMessage());
        }

        int injectCount = 0;
        for (String jarName : targetJars) {
            File jarFile = new File(vfsFrameworkDir + jarName);
            
            if (jarFile.exists() && jarFile.isFile()) {
                Log.d(TAG, "Menyuntikkan biner komponen: " + jarName);
                try {
                    perform_mirror_injection(classLoader, jarFile.getAbsolutePath());
                    injectCount++;
                } catch (Throwable e) {
                    Log.e(TAG, "Gagal menginjeksi komponen " + jarName + ": " + e.getMessage());
                }
            } else {
                Log.w(TAG, "Peringatan: Komponen '" + jarName + "' tidak ditemukan di mount VFS. Melewati...");
            }
        }

        if (injectCount == 0) {
            Log.w(TAG, "Pemberitahuan: Injeksi Java awal ditunda, menunggu kesiapan penuh libvmeer_engine.so di runtime.");
        } else {
            Log.i(TAG, "vMeer OS: Lapisan Framework Berhasil Disatukan. Total: " + injectCount + " JAR.");
        }
    }
}
