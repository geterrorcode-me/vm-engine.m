#ifndef VMEER_CONTEXT_H
#define VMEER_CONTEXT_H

#include <string>
#include <vector>
#include <memory>

namespace vmeer {

class RuntimeContext {
public:
    static RuntimeContext& Get();

    // Lifecycle
    bool Initialize(const std::string& vm_id, const std::string& pkg_name);
    void Heartbeat();

    // State Accessors (Data Only)
    std::string GetAndroidId();
    std::string GetImei();
    const std::vector<uint8_t>& GetMasterSeed();
    int64_t GetTotalUptime();
    int GetBatteryCycles();

private:
    RuntimeContext();
    ~RuntimeContext();
    
    struct Impl;
    std::unique_ptr<Impl> pimpl;
};

} // namespace vmeer

#endif
