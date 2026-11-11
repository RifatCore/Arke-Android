#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <stddef.h>
#include <stdint.h>
#include <errno.h>

#ifdef __cplusplus
extern "C" {
#endif

// 1. Android Bionic errno mapping (glibc uses __errno_location, Bionic uses __errno)
extern int* __errno(void);
__attribute__((visibility("default"))) int* __errno_location(void) {
    return __errno();
}

// 2. Thread-local destructor shim for Bionic
__attribute__((visibility("default"))) int __cxa_thread_atexit_impl(void (*dtor)(void*), void* obj, void* dso_symbol) {
    (void)dtor;
    (void)obj;
    (void)dso_symbol;
    return 0;
}

// 3. Fallback for libc++ hash memory (missing in some NDK r26-r28 builds)
__attribute__((visibility("default"))) size_t _ZNSt6__ndk113__hash_memoryEPKvm(const void* ptr, size_t len) {
    const uint8_t* data = (const uint8_t*)ptr;
    size_t hash = 14695981039346656037ULL; // 64-bit FNV-1a offset basis
    for (size_t i = 0; i < len; ++i) {
        hash ^= (size_t)data[i];
        hash *= 1099511628211ULL;          // 64-bit FNV prime
    }
    return hash;
}

// 4. 64-bit stat compatibility shims for AArch64 Android
__attribute__((visibility("default"))) int stat64(const char* path, struct stat64* buf) {
    return stat(path, (struct stat*)buf);
}

__attribute__((visibility("default"))) int fstat64(int fd, struct stat64* buf) {
    return fstat(fd, (struct stat*)buf);
}

__attribute__((visibility("default"))) int lstat64(const char* path, struct stat64* buf) {
    return lstat(path, (struct stat*)buf);
}

// 5. memfd_create without LIBC_R symbol version dependency
__attribute__((visibility("default"))) int memfd_create(const char* name, unsigned int flags) {
#if defined(__NR_memfd_create)
    return (int)syscall(__NR_memfd_create, name, flags);
#else
    (void)name;
    (void)flags;
    errno = ENOSYS;
    return -1;
#endif
}

#ifdef __cplusplus
}
#endif
