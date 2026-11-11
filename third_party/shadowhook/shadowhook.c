#include "shadowhook.h"
#include <stddef.h>

__attribute__((weak)) int shadowhook_init(int mode, bool debuggable) {
    (void)mode;
    (void)debuggable;
    return SHADOWHOOK_OK;
}

__attribute__((weak)) int shadowhook_get_errno(void) {
    return 0;
}

__attribute__((weak)) const char *shadowhook_to_errmsg(int error_number) {
    (void)error_number;
    return "OK";
}

__attribute__((weak)) void *shadowhook_hook_sym_name(
    const char *lib_name,
    const char *sym_name,
    void *new_addr,
    void **orig_addr
) {
    (void)lib_name;
    (void)sym_name;
    (void)new_addr;
    if (orig_addr) *orig_addr = NULL;
    return NULL;
}

__attribute__((weak)) void *shadowhook_hook_func_addr(
    void *func_addr,
    void *new_addr,
    void **orig_addr
) {
    if (!func_addr || !new_addr) return NULL;
    if (orig_addr) *orig_addr = func_addr;
    return func_addr;
}

__attribute__((weak)) int shadowhook_unhook(void *stub) {
    (void)stub;
    return 0;
}
