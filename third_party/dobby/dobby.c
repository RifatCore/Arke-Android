#define _GNU_SOURCE
#include "dobby.h"
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <dlfcn.h>

// On Android 16 (API 36), raw .text inline code patching triggers SIGSEGV
// due to Execute-Only Memory (XOM) and strict W^X enforcement.
// We preserve original call and safely avoid invalid memory writes.

int DobbyHook(void *address, void *replace_call, void **origin_call) {
    if (!address || !replace_call) return -1;
    if (origin_call) {
        *origin_call = address;
    }
    // Return safely without modifying executable code pages
    return -1;
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
    (void)address;
    return 0;
}
