// PC test stand-in for OpenOrbis's <orbis/libkernel.h>: just the raw file
// calls the ini code makes. tools/test_plugin_config.c provides them.
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
int32_t sceKernelOpen(const char *path, int flags, int mode);
int64_t sceKernelLseek(int32_t fd, int64_t offset, int whence);
ssize_t sceKernelRead(int32_t fd, void *buf, size_t nbyte);
ssize_t sceKernelWrite(int32_t fd, const void *buf, size_t nbyte);
int sceKernelClose(int32_t fd);
