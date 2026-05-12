#include <sys/mman.h>
#include <fcntl.h>
#include "vmeer_helper.h"

namespace vmeer {
    // Menggunakan Shared Memory untuk sinkronisasi state antar proses VM
    void* setup_shared_config(int size) {
        int fd = ashmem_create_region("vmeer_shm", size);
        if (fd < 0) return nullptr;
        return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
}
