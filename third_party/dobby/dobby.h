#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Dobby Hook Core C API for Android ARM64
int DobbyHook(void *address, void *replace_call, void **origin_call);
void *DobbySymbolResolver(const char *image_name, const char *symbol_name);
int DobbyDestroy(void *address);

#define install_hook_name(name, fn_ret_t, fn_args_t...)                                 \
  static fn_ret_t fake_##name(fn_args_t);                                               \
  static fn_ret_t (*orig_##name)(fn_args_t);                                            \
  static void install_hook_##name(void *sym_addr) {                                     \
    DobbyHook(sym_addr, (void *)fake_##name, (void **)&orig_##name);                    \
  }                                                                                     \
  fn_ret_t fake_##name(fn_args_t)

#ifdef __cplusplus
}
#endif
