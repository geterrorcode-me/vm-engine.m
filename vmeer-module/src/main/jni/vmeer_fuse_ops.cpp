#include <fuse.h>
#include <string.h>
#include <errno.h>
#include <android/log.h>

#define LOG_TAG "vMeer_FUSE"

// PENTING: Deklarasikan fungsi dari vmeer_squashfs_bridge.cpp 
// agar compiler tahu fungsi ini ada di file lain.
extern "C" {
    int squash_get_metadata(const char *path, struct stat *stbuf);
    int squash_list_dir(const char *path, void *buf, fuse_fill_dir_t filler);
}

static int vmeer_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi) {
    (void)fi;
    // Sekarang compiler mengenali fungsi ini
    return squash_get_metadata(path, stbuf);
}

static int vmeer_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi,
                         enum fuse_readdir_flags flags) {
    (void)offset; (void)fi; (void)flags;
    return squash_list_dir(path, buf, filler);
}

// Tambahkan definisi operasi FUSE lainnya jika diperlukan
static const struct fuse_operations vmeer_oper = {
    .getattr = vmeer_getattr,
    .readdir = vmeer_readdir,
};
