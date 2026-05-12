// jni/include/ams_runtime.h
namespace vmeer {
namespace ams {
    bool wrap_intent(void* intent_parcel);
    void fix_process_record(int pid, int uid);
}
}
