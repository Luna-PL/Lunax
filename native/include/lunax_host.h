#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int lunax_host_init(int32_t argc, char** argv);
int32_t lunax_host_arg_count(void);
const char* lunax_host_arg(int32_t index);
int32_t lunax_host_text_equals(const char* left, const char* right);
void lunax_host_out_line(const char* text);
void lunax_host_err_line(const char* text);

int32_t lunax_host_download(const char* url, const char* output_path);
int32_t lunax_host_install(const char* kind, const char* version,
                           const char* url);
int32_t lunax_host_list(const char* kind);
int32_t lunax_host_use(const char* kind, const char* version);
int32_t lunax_host_package_fetch(const char* package_id, const char* version,
                                 const char* url);
int32_t lunax_host_package_list(const char* package_id);
int32_t lunax_host_print_env(void);
int32_t lunax_host_print_backend(void);
int32_t lunax_host_print_compiler(void);
int32_t lunax_host_forward_luna(int32_t first_argument);

#ifdef __cplusplus
}
#endif
