#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SHADOWHOOK_OK 0
#define SHADOWHOOK_ERRNO_PENDING -1

typedef void (*shadowhook_hook_cb_t)(int error_number, const char *lib_name, const char *sym_name, void *sym_addr, void *new_addr, void *orig_addr, void *arg);

int shadowhook_init(int mode, bool debuggable);
int shadowhook_get_errno(void);
const char *shadowhook_to_errmsg(int error_number);

void *shadowhook_hook_sym_name(
    const char *lib_name,
    const char *sym_name,
    void *new_addr,
    void **orig_addr
);

void *shadowhook_hook_func_addr(
    void *func_addr,
    void *new_addr,
    void **orig_addr
);

int shadowhook_unhook(void *stub);

#ifdef __cplusplus
}
#endif
