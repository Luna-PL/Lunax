#include "lunax/backend_abi.h"

#include <algorithm>
#include <cstring>

namespace {

int unavailable(void*, const LunaxDownloadRequestV1*, char* error,
                size_t errorCapacity) {
    constexpr char message[] = "test backend does not download";
    if (error && errorCapacity != 0) {
        const size_t copied = std::min(errorCapacity - 1, sizeof(message) - 1);
        std::memcpy(error, message, copied);
        error[copied] = '\0';
    }
    return LUNAX_DOWNLOAD_TRANSPORT_ERROR;
}

const LunaxDownloadBackendV1 kBackend{
    LUNAX_DOWNLOAD_BACKEND_MAGIC_V1,
    LUNAX_DOWNLOAD_BACKEND_ABI_V1,
    sizeof(LunaxDownloadBackendV1),
    0,
    "fake-dynamic",
    nullptr,
    unavailable,
};

} // namespace

#ifdef _WIN32
#define LUNAX_TEST_EXPORT __declspec(dllexport)
#else
#define LUNAX_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" LUNAX_TEST_EXPORT const LunaxDownloadBackendV1*
lunax_download_backend_v1() {
    return &kBackend;
}
