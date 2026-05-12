#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <map>
#include <vector>
#include <mutex>

namespace vmeer {

    // Struktur Data untuk Identitas Virtual proses
    struct VirtualIdentity {
        std::string package_name;
        std::string virtual_process_name;
        int virtual_uid;
        bool is_isolated;
        bool use_virtual_storage;
    };

    class RuntimeContext {
    public:
        // Singleton Pattern
        static RuntimeContext& Get() {
            static RuntimeContext instance;
            return instance;
        }

        // Inisialisasi Environment
        bool Initialize(const std::string& vm_id, const std::string& target_pkg);

        // Registry Management (Evolusi String Descriptor)
        void RegisterVirtualApp(const std::string& pkg, int v_uid);
        bool IsVirtualProcess(const std::string& proc_name);
        VirtualIdentity* GetIdentity(const std::string& proc_name);

        // Virtual Peripherals (Android ID, etc)
        std::string GetVAndroidId() const { return v_android_id_; }
        void Heartbeat(); // Sync ke SQLite

    private:
        RuntimeContext() = default;
        std::map<std::string, VirtualIdentity> registry_;
        std::string v_android_id_;
        std::string master_seed_;
        mutable std::mutex registry_mutex_;

        // Prevent copying
        RuntimeContext(const RuntimeContext&) = delete;
        void operator=(const RuntimeContext&) = delete;
    };

} // namespace vmeer

#endif // VMEER_CONTEXT_H
