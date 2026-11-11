#define _GNU_SOURCE
#include "dobby.h"
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

#define PAGE_START(addr) ((uintptr_t)(addr) & ~((uintptr_t)sysconf(_SC_PAGESIZE) - 1))
#define PAGE_SIZE ((uintptr_t)sysconf(_SC_PAGESIZE))

// Structure to track installed inline hooks for cleanup
struct ActiveHook {
    void *target_address;
    uint8_t orig_instructions[16];
    void *trampoline;
    struct ActiveHook *next;
};

static struct ActiveHook *g_hook_list = NULL;

static void *allocate_trampoline(void) {
    // Allocate RX memory page for trampolines
    void *mem = mmap(NULL, PAGE_SIZE,
                     PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == MAP_FAILED) {
        return NULL;
    }
    return mem;
}

int DobbyHook(void *address, void *replace_call, void **origin_call) {
    if (!address || !replace_call) return -1;

#if defined(__aarch64__)
    uintptr_t target = (uintptr_t)address;
    uintptr_t page_addr = PAGE_START(target);
    size_t map_size = PAGE_SIZE;

    // Check if 16 bytes crosses page boundary
    if ((target + 16) > (page_addr + PAGE_SIZE)) {
        map_size = PAGE_SIZE * 2;
    }

    struct ActiveHook *hook = (struct ActiveHook *)malloc(sizeof(struct ActiveHook));
    if (!hook) return -1;
    hook->target_address = address;
    memcpy(hook->orig_instructions, address, 16);
    hook->trampoline = NULL;

    // Build trampoline if caller requested original function call
    if (origin_call) {
        uint8_t *tramp = (uint8_t *)allocate_trampoline();
        if (tramp) {
            // 1. Copy the original 16 bytes of instructions
            memcpy(tramp, address, 16);

            // 2. Append jump back to target + 16:
            // ldr x16, #8   (0x58000050)
            // br  x16       (0xd61f0200)
            // <64-bit target + 16>
            uint32_t *tramp32 = (uint32_t *)(tramp + 16);
            tramp32[0] = 0x58000050; // ldr x16, #8
            tramp32[1] = 0xd61f0200; // br x16

            uint64_t *return_addr = (uint64_t *)(tramp + 24);
            *return_addr = (uint64_t)(target + 16);

            // Clear icache for trampoline
            __builtin___clear_cache((char *)tramp, (char *)tramp + 32);

            hook->trampoline = tramp;
            *origin_call = tramp;
        } else {
            *origin_call = address;
        }
    }

    // Unprotect target page to write hook
    if (mprotect((void *)page_addr, map_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) {
        free(hook);
        return -1;
    }

    // Write AArch64 absolute branch:
    // ldr x16, #8   (0x58000050)
    // br  x16       (0xd61f0200)
    // <64-bit replace_call>
    uint32_t *code = (uint32_t *)address;
    code[0] = 0x58000050; // ldr x16, #8
    code[1] = 0xd61f0200; // br x16

    uint64_t *dest = (uint64_t *)((uint8_t *)address + 8);
    *dest = (uint64_t)replace_call;

    // Invalidate instruction cache
    __builtin___clear_cache((char *)address, (char *)address + 16);

    // Restore page protection
    mprotect((void *)page_addr, map_size, PROT_READ | PROT_EXEC);

    hook->next = g_hook_list;
    g_hook_list = hook;
    return 0;

#else
    // Fallback for non-aarch64 host builds
    if (origin_call) {
        *origin_call = address;
    }
    return 0;
#endif
}

void *DobbySymbolResolver(const char *image_name, const char *symbol_name) {
    if (!symbol_name) return NULL;

    void *sym = NULL;
    if (image_name && strlen(image_name) > 0) {
        void *handle = dlopen(image_name, RTLD_LAZY | RTLD_NOLOAD);
        if (!handle) {
            handle = dlopen(image_name, RTLD_LAZY);
        }
        if (handle) {
            sym = dlsym(handle, symbol_name);
        }
    }

    if (!sym) {
        sym = dlsym(RTLD_DEFAULT, symbol_name);
    }

    return sym;
}

int DobbyDestroy(void *address) {
    if (!address) return -1;

    struct ActiveHook **curr = &g_hook_list;
    while (*curr) {
        if ((*curr)->target_address == address) {
            struct ActiveHook *to_free = *curr;
            uintptr_t page_addr = PAGE_START(address);

            mprotect((void *)page_addr, PAGE_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC);
            memcpy(address, to_free->orig_instructions, 16);
            __builtin___clear_cache((char *)address, (char *)address + 16);
            mprotect((void *)page_addr, PAGE_SIZE, PROT_READ | PROT_EXEC);

            if (to_free->trampoline) {
                munmap(to_free->trampoline, PAGE_SIZE);
            }

            *curr = to_free->next;
            free(to_free);
            return 0;
        }
        curr = &(*curr)->next;
    }
    return -1;
}
