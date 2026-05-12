#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <mutex>
#include <map>

namespace vmeer {

class RuntimeContext {
public:
    static RuntimeContext& Get() {
        static RuntimeContext instance;
        return instance;
    }

    bool Initialize(const std::string& vm_id, const std::string& target_pkg);
    
    // --- TAMBAHKAN INI AGAR .CPP TIDAK ERROR ---
    void SetVirtualUid(int uid) { m_vuid = uid; }
    void SetMirrorPath(const std::string& path) { m_mirror_path = path; }
    
    int GetVirtualUid() const { return m_vuid; }
    std::string GetMirrorPath() const { return m_mirror_path; }
    // -------------------------------------------

    void Heartbeat();

private:
    RuntimeContext() : m_vuid(0) {}
    int m_vuid;
    std::string m_mirror_path;
    std::string m_target_package;
    std::mutex m_mutex;
};

} // namespace vmeer

#endif
