# ==========================================================
# VMEER ENGINE - PROGUARD CONFIGURATION
# ==========================================================

# 1. Dasar Android & Support Library
-keepattributes SourceFile,LineNumberTable,Signature,InnerClasses,EnclosingMethod,*Annotation*
-keep public class * extends android.app.Service
-keep public class * extends android.app.Application
-keep public class * extends android.view.View

# 2. JNI & NATIVE BRIDGE (SANGAT PENTING)
# Jangan ubah nama method yang dipanggil oleh C++ melalui JNI
-keepclasseswithmembernames class * {
    native <methods>;
}

# Jangan ubah nama class yang direferensikan dalam kode C++ (vmeer_jni_bridge.cpp)
-keep class com.vmeer.core.NativeEngine { *; }
-keep class com.vmeer.core.VMeerService { *; }

# 3. ANDROID FRAMEWORK MIRROR (mirror.jar)
# Karena kamu menggunakan bytecode remapping ke black.android.*
# Kita harus menjaga agar struktur paket ini tetap utuh
-keep class black.android.** { *; }
-keep interface black.android.** { *; }

# 4. SHADOWHOOK & LIBRARIES
# Menjaga fungsionalitas hooking agar tidak rusak saat runtime
-keep class com.bytedance.shadowhook.** { *; }
-dontwarn com.bytedance.shadowhook.**

# 5. SQLITE & ZSTD
# Menghindari error pada library database dan kompresi
-keep class org.sqlite.** { *; }
-keep class com.facebook.zstd.** { *; }

# 6. REFLECTION HELPER
# Jika vMeer menggunakan refleksi untuk mengakses internal API (Hidden API)
-keep class android.os.** { *; }
-keep class android.app.** { *; }
-keep class android.content.pm.** { *; }

# 7. MODEL DATA & DATABASE (vmeer_db.cpp)
# Jika kamu punya POJO/Data class yang dipetakan ke SQLite
-keepclassmembers class **.models.** {
    private <fields>;
    public <methods>;
}

# 8. PENGEMBANGAN (REMOVING LOGS)
# Aktifkan ini jika ingin menghapus log otomatis pada versi Release
# -assumenosideeffects class android.util.Log {
#     public static int v(...);
#     public static int d(...);
#     public static int i(...);
#     public static int w(...);
#     public static int e(...);
# }

# 9. OPTIMASI TAMBAHAN
-dontoptimize
-dontobfuscate # Gunakan ini HANYA jika engine masih crash setelah diproteksi
-ignorewarnings
