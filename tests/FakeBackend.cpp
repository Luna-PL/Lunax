#include "lunax/backend_abi.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>

namespace {

int copyFixture(void*, const LunaxDownloadRequestV1* request, char* error,
                size_t errorCapacity) {
    const char* fixture = std::getenv("LUNAX_TEST_ARCHIVE");
    const char* message = nullptr;
    if (!request || !request->output_path_utf8 || !fixture || !*fixture) {
        message = "LUNAX_TEST_ARCHIVE is not configured";
    } else {
        std::ifstream input(fixture, std::ios::binary);
        std::ofstream output(request->output_path_utf8,
                             std::ios::binary | std::ios::trunc);
        if (input && output) output << input.rdbuf();
        if (input && output) return LUNAX_DOWNLOAD_OK;
        message = "test backend could not copy the fixture archive";
    }
    if (error && errorCapacity != 0 && message) {
        const size_t length = std::strlen(message);
        const size_t copied = std::min(errorCapacity - 1, length);
        std::memcpy(error, message, copied);
        error[copied] = '\0';
    }
    return LUNAX_DOWNLOAD_FILESYSTEM_ERROR;
}

const LunaxDownloadBackendV1 kBackend{
    LUNAX_DOWNLOAD_BACKEND_MAGIC_V1,
    LUNAX_DOWNLOAD_BACKEND_ABI_V1,
    sizeof(LunaxDownloadBackendV1),
    0,
    "fake-dynamic",
    nullptr,
    copyFixture,
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
