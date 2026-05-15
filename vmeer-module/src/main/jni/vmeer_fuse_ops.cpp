#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <errno.h>
#include <string.h>

// Fungsi untuk mengecek atribut file (stat)
static int vmeer_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    memset(stbuf, 0, sizeof(struct stat));

    // BYPASS LOGIC: Sembunyikan file SU (Root)
    if (strstr(path, "bin/su") || strstr(path, "xbin/su")) {
        return -ENOENT; // Katakan: File Tidak Ada!
    }

    // Default: Ambil metadata dari readonly.bin via libsquashfs
    return squash_get_metadata(path, stbuf);
}

static int vmeer_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags) {
    // List file yang ada di dalam SquashFS
    return squash_list_dir(path, buf, filler);
}

static struct fuse_operations vmeer_oper = {
    .getattr = vmeer_getattr,
    .readdir = vmeer_readdir,
    // .read = vmeer_read_file,
};

void vmeer_fuse_init(const char* rom, const char* mnt) {
    char *argv[] = {(char*)"vmeer", (char*)"-f", (char*)mnt, NULL};
    fuse_main(3, argv, &vmeer_oper, (void*)rom);
}
