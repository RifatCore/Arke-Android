#include "dobby.h"
#include <stddef.h>

__attribute__((weak)) int DobbyHook(void *address, void *replace_call, void **origin_call) {
    if (!address || !replace_call) return -1;
    if (origin_call) {
        *origin_call = address;
    }
    return 0;
}

__attribute__((weak)) void *DobbySymbolResolver(const char *image_name, const char *symbol_name) {
    (void)image_name;
    (void)symbol_name;
    return NULL;
}

__attribute__((weak)) int DobbyDestroy(void *address) {
    (void)address;
    return 0;
}
