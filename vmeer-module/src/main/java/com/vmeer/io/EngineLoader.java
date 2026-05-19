package com.vmeer.io;

import android.content.Context;
import android.util.Log;
import dalvik.system.InMemoryDexClassLoader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.ByteBuffer;

public class EngineLoader {
    private static final String TAG = "vMeer_Bridge";
    private static boolean isNativeLoaded = false;

    // 🛡️ FIX CRITICAL: Pemuatan seluruh library .so wajib berada di urutan paling awal
    static {
        try {
            System.loadLibrary("shadowhook");
            System.loadLibrary("romfs_vm");
            System.loadLibrary("binder_vm");
            System.loadLibrary("art_vm");
            System.loadLibrary("surface_vm_standalone");
            System.loadLibrary("vmeer_engine");
            isNativeLoaded = true;
            Log.i(TAG, "vMeer Bootstrap: Seluruh pustaka native (.so) berhasil dimuat di awal.");
        } catch (UnsatisfiedLinkError e) {
            Log.e(TAG, "vMeer Bootstrap: FATAL - Gagal memuat library native!", e);
        }
    }

    // Ekspor Fungsi JNI Native (Akan ditangkap oleh vmeer_engine.cpp)
    public static native void init_art_hook();
    public static native void perform_mirror_injection(ClassLoader classLoader, String jarPath);

    /**
     * Solusi Modern: Membuat ClassLoader terisolasi dari penyangga memori (RAM murni)
     * Menggunakan logika gabungan InMemory + Delegate Last agar terhindar dari conflict classpath.
     */
    public static ClassLoader createVirtualClassLoader(ByteBuffer[] dexBuffers, ClassLoader parent) {
        Log.i(TAG, "vMeer OS: Membangun Custom InMemoryDexClassLoader untuk Sandbox Aplikasi...");
        
        return new InMemoryDexClassLoader(dexBuffers, parent) {
            @Override
            protected Class<?> loadClass(String name, boolean resolve) throws ClassNotFoundException {
                // 1. Cek cache lokal memori runtime
                Class<?> c = findLoadedClass(name);
                if (c != null) return c;

                // 2. [DELEGATE LAST LOGIC] Paksa cari di dalam biner aplikasi tamu dulu
                try {
                    return findClass(name);
                } catch (ClassNotFoundException e) {
                    // Abaikan, lanjut ke langkah berikutnya jika tidak ada di dalam DEX tamu
                }

                // 3. Jika tidak ketemu di tamu, delegasikan ke sistem host
                return super.loadClass(name, resolve);
            }
        };
    }

    /**
     * Memastikan biner vmeerd ter-deploy ke folder target mutlak: app_app_bin
     */
    private static void deployDaemonFromAssets(Context context, File targetDaemonFile) {
        File parentDir = targetDaemonFile.getParentFile();
        if (parentDir != null && !parentDir.exists()) {
            parentDir.mkdirs();
        }

        if (!targetDaemonFile.exists()) {
            Log.i(TAG, "vMeer Bootstrap: Menggandakan biner vmeerd dari assets ke target mutlak...");
            try (InputStream in = context.getAssets().open("vmeer_bin/vmeerd");
                 OutputStream out = new FileOutputStream(targetDaemonFile)) {
                
                byte[] buffer = new byte[8192];
                int read;
                while ((read = in.read(buffer)) != -1) {
                    out.write(buffer, 0, read);
                }
                out.flush();
                Log.i(TAG, "vMeer Bootstrap: Deployment vmeerd ke app_app_bin SUKSES.");
                
            } catch (IOException e) {
                Log.e(TAG, "vMeer Bootstrap: CRITICAL! Gagal deploy vmeerd: " + e.getMessage());
                return;
            }
        }

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
            vfsFrameworkDir.mkdirs();
        }

        String[] jarFiles = {"core-oj.jar", "core-libart.jar", "ext.jar", "framework.jar", "services.jar"};

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
                    Log.e(TAG, "vMeer Assets: Gagal mengekstrak komponen framework: " + jar);
                    continue;
                }
            }

            if (targetFile.exists()) {
                targetFile.setReadOnly();
                targetFile.setWritable(false, false);
                Log.d(TAG, "vMeer Safety: Mengunci berkas " + jar + " -> ReadOnly & Revoke Writable.");
            }
        }
    }

    /**
     * Membangunkan biner vmeerd murni dari folder internal app_app_bin
     */
    private static void awakenDaemonProcess(Context context) {
        try {
            String baseDataDir = context.getFilesDir().getParent();
            File daemonFile = new File(baseDataDir + "/app_app_bin/vmeerd");
            
            deployDaemonFromAssets(context, daemonFile);
            
            if (daemonFile.exists()) {
                Log.i(TAG, "vMeer Bootstrap: Membangunkan native daemon vmeerd dari -> " + daemonFile.getAbsolutePath());
                Runtime.getRuntime().exec(new String[]{daemonFile.getAbsolutePath()});
                Thread.sleep(350); // Jeda sinkronisasi IPC kernel socket
            }
        } catch (Exception e) {
            Log.e(TAG, "vMeer Bootstrap: Gagal memicu eksekusi daemon: " + e.getMessage());
        }
    }

    /**
     * Titik entri utama Lapisan 2 untuk menyatukan struktur Java Runtime Framework kontainer
     */
    public static void startFrameworkInjection(Context context, ClassLoader classLoader) {
        if (!isNativeLoaded) {
            Log.e(TAG, "Injeksi Framework Dibatalkan: libvmeer_engine.so gagal dimuat.");
            return;
        }

        awakenDaemonProcess(context);
        
        String baseDataDir = context.getFilesDir().getParent();
        String vfsFrameworkDir = baseDataDir + "/app_app_bin/rootfs/system/framework/";

        Log.i(TAG, "vMeer OS: Memeriksa kesiapan berkas fisik Java Framework di VFS -> " + vfsFrameworkDir);
        extractFrameworkFromAssets(context, vfsFrameworkDir);
        
        String[] targetJars = {"core-oj.jar", "core-libart.jar", "ext.jar", "framework.jar", "services.jar"};
        Log.i(TAG, "vMeer OS: Memulai Lapisan 2 - Runtime Framework Injection...");

        // Eksekusi aman JNI Hook karena library .so dipastikan sudah siap siaga
        try {
            init_art_hook();
        } catch (Throwable t) {
            Log.e(TAG, "Gagal mengeksekusi init_art_hook via JNI murni: " + t.getMessage());
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
            }
        }
        Log.i(TAG, "vMeer OS: Lapisan Framework Berhasil Disatukan. Total: " + injectCount + " JAR.");
    }
}
