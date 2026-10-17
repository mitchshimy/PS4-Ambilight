// Desktop stand-ins for sceKernel* file I/O, so config.c (which calls these directly, matching the...
// More: docs/code-comments.md (companion-app/tools/sce_file_stubs.c:1)
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>

int32_t sceKernelOpen(const char *path, int flags, int mode) {
    int posixFlags = 0;
    if (flags == 0) posixFlags = O_RDONLY;
    else posixFlags = O_WRONLY | O_CREAT | O_TRUNC; // matches the plugin's own 0x200|0x001 write pattern loosely enough for this test
    return open(path, posixFlags, mode);
}
int64_t sceKernelLseek(int32_t fd, int64_t offset, int whence) { return lseek(fd, offset, whence); }
ssize_t sceKernelRead(int32_t fd, void *buf, size_t nbyte) { return read(fd, buf, nbyte); }
ssize_t sceKernelWrite(int32_t fd, const void *buf, size_t nbyte) { return write(fd, buf, nbyte); }
int sceKernelClose(int32_t fd) { return close(fd); }
