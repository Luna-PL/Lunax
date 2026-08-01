#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LUNAX_DOWNLOAD_BACKEND_ABI_V1 1u
#define LUNAX_DOWNLOAD_BACKEND_MAGIC_V1 UINT32_C(0x4c584231) /* LXB1 */
#define LUNAX_DOWNLOAD_BACKEND_ENTRY_V1 "lunax_download_backend_v1"

enum LunaxDownloadStatusV1 {
    LUNAX_DOWNLOAD_OK = 0,
    LUNAX_DOWNLOAD_INVALID_ARGUMENT = -1,
    LUNAX_DOWNLOAD_UNSUPPORTED_ABI = -2,
    LUNAX_DOWNLOAD_OUTPUT_EXISTS = -3,
    LUNAX_DOWNLOAD_TRANSPORT_ERROR = -4,
    LUNAX_DOWNLOAD_FILESYSTEM_ERROR = -5,
};

enum LunaxDownloadFlagV1 {
    LUNAX_DOWNLOAD_FOLLOW_REDIRECTS = UINT64_C(1) << 0,
    LUNAX_DOWNLOAD_REQUIRE_TLS_VERIFICATION = UINT64_C(1) << 1,
    LUNAX_DOWNLOAD_REJECT_OVERWRITE = UINT64_C(1) << 2,
};

typedef struct LunaxDownloadRequestV1 {
    uint32_t abi_version;
    uint32_t struct_size;
    const char* url_utf8;
    const char* output_path_utf8;
    uint64_t flags;
    uint64_t connect_timeout_seconds;
    uint64_t transfer_timeout_seconds;
} LunaxDownloadRequestV1;

typedef int (*LunaxDownloadFnV1)(
    void* context,
    const LunaxDownloadRequestV1* request,
    char* error_utf8,
    size_t error_capacity);

typedef struct LunaxDownloadBackendV1 {
    uint32_t magic;
    uint32_t abi_version;
    uint32_t struct_size;
    uint32_t reserved_zero;
    const char* name;
    void* context;
    LunaxDownloadFnV1 download;
} LunaxDownloadBackendV1;

// Dynamic backends export this exact symbol. The descriptor and its context
// remain valid until process exit; Lunax keeps the shared library loaded.
typedef const LunaxDownloadBackendV1* (*LunaxDownloadBackendEntryV1)(void);

#ifdef __cplusplus
}
#endif
