# ==========================================================
# VMEER MODULE - CONSUMER RULES
# (Aturan ini akan otomatis diterapkan pada aplikasi pengguna)
# ==========================================================

# 1. JAGA INTERFACE JNI (CORE ENGINE)
# Pastikan method native dan class bridge tidak hilang atau diubah namanya
-keepclasseswithmembernames class com.vmeer.core.NativeEngine {
    native <methods>;
}

-keep class com.vmeer.core.NativeEngine { *; }
-keep class com.vmeer.core.VMeerService { *; }

# 2. FRAMEWORK MIRROR (mirror.jar)
# Sangat krusial: Jika aplikasi pengguna melakukan obfuscation, 
# paket black.android.* tidak boleh disentuh karena C++ mencarinya secara hardcoded.
-keep class black.android.** { *; }
-keep interface black.android.** { *; }

# 3. NATIVE LIBRARY LOADING
# Memastikan library native (.so) tetap bisa ditemukan dan dimuat
-keepattributes Native, EnclosingMethod, InnerClasses, Signature, *Annotation*

# 4. SHADOWHOOK
# Shadowhook melakukan manipulasi memori dan bytecode, jangan biarkan ProGuard merusaknya
-keep class com.bytedance.shadowhook.** { *; }
-dontwarn com.bytedance.shadowhook.**

# 5. THIRD-PARTY COMPATIBILITY
# SQLITE: Jika digunakan dalam konteks JNI/Native
-keep class org.sqlite.** { *; }

# 6. SUPPRESS WARNINGS
# Menghindari kegagalan build pada aplikasi pengguna jika ada library yang tidak lengkap
-ignorewarnings
-dontnote com.vmeer.core.**
