#define _GNU_SOURCE
#include "shadowhook.h"
#include "dobby/dobby.h"
#include <link.h>
#include <elf.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static inline size_t get_os_pagesize(void) {
    long ps = sysconf(_SC_PAGESIZE);
    return (ps > 0) ? (size_t)ps : 4096;
}

#define PAGE_START(addr, ps) ((uintptr_t)(addr) & ~((uintptr_t)(ps) - 1))

struct HookContext {
    const char *target_lib;
    const char *target_sym;
    void *new_addr;
    void **orig_addr;
    bool hooked;
};

static int elf_hook_callback(struct dl_phdr_info *info, size_t size, void *data) {
    (void)size;
    struct HookContext *ctx = (struct HookContext *)data;
    if (!info || !info->dlpi_name) return 0;

    // Check library name match (substring or basename)
    if (ctx->target_lib && strlen(ctx->target_lib) > 0) {
        if (!strstr(info->dlpi_name, ctx->target_lib)) {
            return 0;
        }
    }

    uintptr_t load_bias = info->dlpi_addr;
    const ElfW(Phdr) *phdr = info->dlpi_phdr;
    const ElfW(Dyn) *dyn = NULL;

    for (int i = 0; i < info->dlpi_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            dyn = (const ElfW(Dyn) *)(load_bias + phdr[i].p_vaddr);
            break;
        }
    }

    if (!dyn) return 0;

    const ElfW(Rela) *rela_plt = NULL;
    size_t rela_plt_sz = 0;
    const ElfW(Rela) *rela_dyn = NULL;
    size_t rela_dyn_sz = 0;
    const ElfW(Sym) *symtab = NULL;
    const char *strtab = NULL;

    for (const ElfW(Dyn) *d = dyn; d->d_tag != DT_NULL; d++) {
        switch (d->d_tag) {
            case DT_JMPREL:
                rela_plt = (const ElfW(Rela) *)(load_bias + d->d_un.d_ptr);
                break;
            case DT_PLTRELSZ:
                rela_plt_sz = d->d_un.d_val;
                break;
            case DT_RELA:
                rela_dyn = (const ElfW(Rela) *)(load_bias + d->d_un.d_ptr);
                break;
            case DT_RELASZ:
                rela_dyn_sz = d->d_un.d_val;
                break;
            case DT_SYMTAB:
                symtab = (const ElfW(Sym) *)(load_bias + d->d_un.d_ptr);
                break;
            case DT_STRTAB:
                strtab = (const char *)(load_bias + d->d_un.d_ptr);
                break;
        }
    }

    if (!strtab || !symtab) return 0;

    // Search both rela.plt and rela.dyn
    const ElfW(Rela) *reloc_tables[] = { rela_plt, rela_dyn };
    size_t reloc_sizes[] = { rela_plt_sz, rela_dyn_sz };

    size_t page_sz = get_os_pagesize();

    for (int t = 0; t < 2; t++) {
        const ElfW(Rela) *table = reloc_tables[t];
        size_t bytes = reloc_sizes[t];
        if (!table || bytes == 0) continue;

        size_t count = bytes / sizeof(ElfW(Rela));
        for (size_t i = 0; i < count; i++) {
            uint32_t sym_idx = ELF64_R_SYM(table[i].r_info);
            uint32_t type = ELF64_R_TYPE(table[i].r_info);

            if (type != R_AARCH64_JUMP_SLOT && type != R_AARCH64_GLOB_DAT) {
                continue;
            }

            const char *sym_name = strtab + symtab[sym_idx].st_name;
            if (strcmp(sym_name, ctx->target_sym) == 0) {
                void **got_entry = (void **)(load_bias + table[i].r_offset);

                uintptr_t page_start = PAGE_START(got_entry, page_sz);
                if (mprotect((void *)page_start, page_sz, PROT_READ | PROT_WRITE) == 0) {
                    if (ctx->orig_addr && *ctx->orig_addr == NULL) {
                        *ctx->orig_addr = *got_entry;
                    }
                    *got_entry = ctx->new_addr;
                    __builtin___clear_cache((char *)got_entry, (char *)got_entry + sizeof(void *));
                    mprotect((void *)page_start, page_sz, PROT_READ);
                    ctx->hooked = true;
                    return 1; // Done hooking this occurrence
                }
            }
        }
    }

    return 0;
}

int shadowhook_init(int mode, bool debuggable) {
    (void)mode;
    (void)debuggable;
    return SHADOWHOOK_OK;
}

int shadowhook_get_errno(void) {
    return 0;
}

const char *shadowhook_to_errmsg(int error_number) {
    (void)error_number;
    return "OK";
}

void *shadowhook_hook_sym_name(
    const char *lib_name,
    const char *sym_name,
    void *new_addr,
    void **orig_addr
) {
    if (!sym_name || !new_addr) return NULL;

    struct HookContext ctx;
    ctx.target_lib = lib_name;
    ctx.target_sym = sym_name;
    ctx.new_addr = new_addr;
    ctx.orig_addr = orig_addr;
    ctx.hooked = false;

    dl_iterate_phdr(elf_hook_callback, &ctx);

    if (ctx.hooked) {
        return new_addr;
    }
    return NULL;
}

void *shadowhook_hook_func_addr(
    void *func_addr,
    void *new_addr,
    void **orig_addr
) {
    if (!func_addr || !new_addr) return NULL;
    // On Android 16 / XOM, inline patching is disabled for stability
    if (orig_addr) {
        *orig_addr = func_addr;
    }
    return NULL;
}

int shadowhook_unhook(void *stub) {
    (void)stub;
    return 0;
}
