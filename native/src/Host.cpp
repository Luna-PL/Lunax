#include "lunax_host.h"

#include "lunax/backend_abi.h"

#include <curl/curl.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

constexpr int kUsageError = 2;
constexpr int kConfigurationError = 3;
constexpr int kDownloadError = 4;
constexpr int kFilesystemError = 5;

int gArgc = 0;
char** gArgv = nullptr;

const char* environment(const char* name) {
    const char* value = std::getenv(name);
    return value && *value ? value : nullptr;
}

fs::path lunaxHome() {
    if (const char* configured = environment("LUNAX_HOME"))
        return fs::u8path(configured);
#ifdef _WIN32
    if (const char* local = environment("LOCALAPPDATA"))
        return fs::u8path(local) / "Lunax";
#endif
    if (const char* home = environment("HOME"))
        return fs::u8path(home) / ".lunax";
    return fs::current_path() / ".lunax";
}

bool validKind(const std::string& kind) {
    return kind == "compiler" || kind == "toolchain" || kind == "sdk";
}

bool validVersion(const std::string& version) {
    if (version.empty()) return false;
    return std::all_of(version.begin(), version.end(), [](unsigned char byte) {
        return (byte >= 'a' && byte <= 'z') ||
            (byte >= 'A' && byte <= 'Z') ||
            (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' ||
            byte == '-';
    });
}

bool validPackageId(const std::string& packageId) {
    if (packageId.empty()) return false;
    return std::all_of(
        packageId.begin(), packageId.end(), [](unsigned char byte) {
            return (byte >= 'a' && byte <= 'z') ||
                (byte >= 'A' && byte <= 'Z') ||
                (byte >= '0' && byte <= '9') || byte == '.' || byte == '_' ||
                byte == '-';
        });
}

std::string pluralKind(const std::string& kind) {
    if (kind == "toolchain") return "toolchains";
    if (kind == "compiler") return "compilers";
    return "sdks";
}

fs::path installRoot(const std::string& kind, const std::string& version) {
    return lunaxHome() / pluralKind(kind) / version;
}

fs::path activeFile(const std::string& kind) {
    return lunaxHome() / "active" / kind;
}

std::string readActive(const std::string& kind) {
    std::ifstream input(activeFile(kind));
    std::string version;
    std::getline(input, version);
    return validVersion(version) ? version : std::string{};
}

void reportFilesystem(const std::string& action, const fs::path& path,
                      const std::error_code& error) {
    std::cerr << "lunax: " << action << " '" << path.string()
              << "': " << error.message() << '\n';
}

void writeBackendError(char* output, size_t capacity,
                       const std::string& message) {
    if (!output || capacity == 0) return;
    const size_t copied = std::min(capacity - 1, message.size());
    std::memcpy(output, message.data(), copied);
    output[copied] = '\0';
}

size_t curlWrite(void* data, size_t size, size_t count, void* context) {
    return std::fwrite(data, size, count, static_cast<FILE*>(context));
}

int curlDownload(void*, const LunaxDownloadRequestV1* request,
                 char* error, size_t errorCapacity) {
    if (!request || request->abi_version != LUNAX_DOWNLOAD_BACKEND_ABI_V1 ||
        request->struct_size < sizeof(LunaxDownloadRequestV1) ||
        !request->url_utf8 || !*request->url_utf8 ||
        !request->output_path_utf8 || !*request->output_path_utf8) {
        writeBackendError(error, errorCapacity, "invalid download request");
        return LUNAX_DOWNLOAD_INVALID_ARGUMENT;
    }

    const fs::path output = fs::u8path(request->output_path_utf8);
    std::error_code filesystemError;
    if ((request->flags & LUNAX_DOWNLOAD_REJECT_OVERWRITE) != 0 &&
        fs::exists(output, filesystemError)) {
        writeBackendError(error, errorCapacity,
                          "destination already exists; Lunax does not overwrite downloads");
        return LUNAX_DOWNLOAD_OUTPUT_EXISTS;
    }
    if (!output.parent_path().empty()) {
        fs::create_directories(output.parent_path(), filesystemError);
        if (filesystemError) {
            writeBackendError(error, errorCapacity, filesystemError.message());
            return LUNAX_DOWNLOAD_FILESYSTEM_ERROR;
        }
    }

    FILE* destination = std::fopen(output.string().c_str(), "wb");
    if (!destination) {
        writeBackendError(error, errorCapacity,
                          "cannot create destination: " +
                              std::string(std::strerror(errno)));
        return LUNAX_DOWNLOAD_FILESYSTEM_ERROR;
    }

    static const int curlInitialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (curlInitialized != CURLE_OK) {
        std::fclose(destination);
        writeBackendError(error, errorCapacity, "libcurl global initialization failed");
        return LUNAX_DOWNLOAD_TRANSPORT_ERROR;
    }
    CURL* transfer = curl_easy_init();
    if (!transfer) {
        std::fclose(destination);
        writeBackendError(error, errorCapacity, "libcurl transfer initialization failed");
        return LUNAX_DOWNLOAD_TRANSPORT_ERROR;
    }

    curl_easy_setopt(transfer, CURLOPT_URL, request->url_utf8);
    curl_easy_setopt(transfer, CURLOPT_WRITEFUNCTION, curlWrite);
    curl_easy_setopt(transfer, CURLOPT_WRITEDATA, destination);
    curl_easy_setopt(transfer, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(transfer, CURLOPT_USERAGENT, "lunax/0.1.0");
    curl_easy_setopt(transfer, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(
        transfer, CURLOPT_FOLLOWLOCATION,
        (request->flags & LUNAX_DOWNLOAD_FOLLOW_REDIRECTS) != 0 ? 1L : 0L);
    curl_easy_setopt(
        transfer, CURLOPT_SSL_VERIFYPEER,
        (request->flags & LUNAX_DOWNLOAD_REQUIRE_TLS_VERIFICATION) != 0
            ? 1L : 0L);
    curl_easy_setopt(
        transfer, CURLOPT_SSL_VERIFYHOST,
        (request->flags & LUNAX_DOWNLOAD_REQUIRE_TLS_VERIFICATION) != 0
            ? 2L : 0L);
    if (request->connect_timeout_seconds != 0)
        curl_easy_setopt(transfer, CURLOPT_CONNECTTIMEOUT,
                         static_cast<long>(request->connect_timeout_seconds));
    if (request->transfer_timeout_seconds != 0)
        curl_easy_setopt(transfer, CURLOPT_TIMEOUT,
                         static_cast<long>(request->transfer_timeout_seconds));
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(transfer, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(transfer, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(transfer, CURLOPT_PROTOCOLS,
                     CURLPROTO_HTTP | CURLPROTO_HTTPS);
    curl_easy_setopt(transfer, CURLOPT_REDIR_PROTOCOLS,
                     CURLPROTO_HTTP | CURLPROTO_HTTPS);
#endif

    const CURLcode result = curl_easy_perform(transfer);
    curl_easy_cleanup(transfer);
    const int closeResult = std::fclose(destination);
    if (result != CURLE_OK) {
        writeBackendError(error, errorCapacity, curl_easy_strerror(result));
        return LUNAX_DOWNLOAD_TRANSPORT_ERROR;
    }
    if (closeResult != 0) {
        writeBackendError(error, errorCapacity,
                          "cannot close downloaded file: " +
                              std::string(std::strerror(errno)));
        return LUNAX_DOWNLOAD_FILESYSTEM_ERROR;
    }
    return LUNAX_DOWNLOAD_OK;
}

const LunaxDownloadBackendV1 kCurlBackend{
    LUNAX_DOWNLOAD_BACKEND_MAGIC_V1,
    LUNAX_DOWNLOAD_BACKEND_ABI_V1,
    sizeof(LunaxDownloadBackendV1),
    0,
    "curl",
    nullptr,
    curlDownload,
};

struct SelectedBackend {
    const LunaxDownloadBackendV1* descriptor = nullptr;
    void* library = nullptr;
    std::string error;
};

bool validBackend(const LunaxDownloadBackendV1* backend) {
    return backend && backend->magic == LUNAX_DOWNLOAD_BACKEND_MAGIC_V1 &&
        backend->abi_version == LUNAX_DOWNLOAD_BACKEND_ABI_V1 &&
        backend->struct_size >= sizeof(LunaxDownloadBackendV1) &&
        backend->reserved_zero == 0 && backend->name && *backend->name &&
        backend->download;
}

std::vector<fs::path> backendCandidates(const std::string& name) {
    std::vector<fs::path> candidates;
    const fs::path requested = fs::u8path(name);
    if (requested.has_parent_path() || requested.has_extension()) {
        candidates.push_back(requested);
        return candidates;
    }
#ifdef _WIN32
    const std::string file = "lunax_backend_" + name + ".dll";
    constexpr char separator = ';';
#elif defined(__APPLE__)
    const std::string file = "liblunax_backend_" + name + ".dylib";
    constexpr char separator = ':';
#else
    const std::string file = "liblunax_backend_" + name + ".so";
    constexpr char separator = ':';
#endif
    if (const char* configured = environment("LUNAX_BACKEND_PATH")) {
        std::string paths(configured);
        size_t begin = 0;
        while (begin <= paths.size()) {
            const size_t end = paths.find(separator, begin);
            const std::string path = paths.substr(
                begin, end == std::string::npos ? std::string::npos
                                                : end - begin);
            if (!path.empty()) candidates.push_back(fs::u8path(path) / file);
            if (end == std::string::npos) break;
            begin = end + 1;
        }
    }
    candidates.push_back(lunaxHome() / "backends" / file);
    candidates.push_back(fs::u8path(file));
    return candidates;
}

SelectedBackend loadBackend() {
    const std::string selected = environment("LUNAX_DOWNLOAD_BACKEND")
        ? environment("LUNAX_DOWNLOAD_BACKEND") : "curl";
    if (selected == "curl") return {&kCurlBackend, nullptr, {}};

    std::string lastError = "backend library was not found";
    for (const fs::path& candidate : backendCandidates(selected)) {
#ifdef _WIN32
        HMODULE library = LoadLibraryW(candidate.wstring().c_str());
        if (!library) continue;
        auto entry = reinterpret_cast<LunaxDownloadBackendEntryV1>(
            GetProcAddress(library, LUNAX_DOWNLOAD_BACKEND_ENTRY_V1));
        if (!entry) {
            lastError = "backend does not export "
                LUNAX_DOWNLOAD_BACKEND_ENTRY_V1;
            FreeLibrary(library);
            continue;
        }
        const LunaxDownloadBackendV1* descriptor = entry();
        if (validBackend(descriptor))
            return {descriptor, reinterpret_cast<void*>(library), {}};
        FreeLibrary(library);
#else
        void* library = dlopen(candidate.string().c_str(), RTLD_NOW | RTLD_LOCAL);
        if (!library) {
            if (const char* error = dlerror()) lastError = error;
            continue;
        }
        dlerror();
        auto entry = reinterpret_cast<LunaxDownloadBackendEntryV1>(
            dlsym(library, LUNAX_DOWNLOAD_BACKEND_ENTRY_V1));
        if (const char* error = dlerror()) {
            lastError = error;
            dlclose(library);
            continue;
        }
        const LunaxDownloadBackendV1* descriptor = entry();
        if (validBackend(descriptor)) return {descriptor, library, {}};
        dlclose(library);
#endif
        lastError = "backend descriptor is incompatible with Lunax ABI v1";
    }
    return {nullptr, nullptr, selected + ": " + lastError};
}

const SelectedBackend& selectedBackend() {
    static const SelectedBackend backend = loadBackend();
    return backend;
}

int downloadTo(const std::string& url, const fs::path& output) {
    const SelectedBackend& backend = selectedBackend();
    if (!backend.descriptor) {
        std::cerr << "lunax: cannot load download backend: "
                  << backend.error << '\n';
        return kDownloadError;
    }
    const std::string outputText = output.string();
    const LunaxDownloadRequestV1 request{
        LUNAX_DOWNLOAD_BACKEND_ABI_V1,
        sizeof(LunaxDownloadRequestV1),
        url.c_str(),
        outputText.c_str(),
        LUNAX_DOWNLOAD_FOLLOW_REDIRECTS |
            LUNAX_DOWNLOAD_REQUIRE_TLS_VERIFICATION |
            LUNAX_DOWNLOAD_REJECT_OVERWRITE,
        30,
        0,
    };
    char error[512]{};
    const int status = backend.descriptor->download(
        backend.descriptor->context, &request, error, sizeof(error));
    if (status != LUNAX_DOWNLOAD_OK) {
        std::cerr << "lunax: download failed via " << backend.descriptor->name
                  << " (" << status << "): "
                  << (error[0] ? error : "no backend diagnostic") << '\n';
        if (status != LUNAX_DOWNLOAD_OUTPUT_EXISTS)
            std::cerr << "lunax: partial output, if any, was retained at '"
                      << output.string() << "'\n";
        return kDownloadError;
    }
    return 0;
}

int setEnvironment(const std::string& name, const std::string& value) {
#ifdef _WIN32
    return _putenv_s(name.c_str(), value.c_str());
#else
    return setenv(name.c_str(), value.c_str(), 1);
#endif
}

std::string executableName(const fs::path& root) {
#ifdef _WIN32
    return (root / "bin" / "luna.exe").string();
#else
    return (root / "bin" / "luna").string();
#endif
}

std::string selectedCompiler() {
    if (const char* explicitCompiler = environment("LUNAX_LUNA"))
        return explicitCompiler;
    const std::string active = readActive("compiler");
    if (!active.empty()) {
        const fs::path root = installRoot("compiler", active);
        std::error_code error;
        const fs::path executable = executableName(root);
        if (fs::is_regular_file(executable, error)) return executable.string();
    }
    return "luna";
}

void applyManagedEnvironment() {
    const fs::path home = lunaxHome();
    setEnvironment("LUNAX_HOME", home.string());
    setEnvironment("LUNA_PACKAGE_HOME", (home / "packages").string());

    const std::string compiler = readActive("compiler");
    if (!compiler.empty()) {
        const fs::path root = installRoot("compiler", compiler);
        setEnvironment("LUNA_HOME", root.string());
        setEnvironment("LUNA_RUNTIME_LIB",
                       (root / "lib" / "libruntime.a").string());
        if (const char* oldPath = std::getenv("PATH")) {
#ifdef _WIN32
            const char separator = ';';
#else
            const char separator = ':';
#endif
            setEnvironment("PATH", (root / "bin").string() + separator + oldPath);
        }
    }
    const std::string toolchain = readActive("toolchain");
    if (!toolchain.empty())
        setEnvironment("LUNA_TOOLCHAIN",
                       installRoot("toolchain", toolchain).string());
    const std::string sdk = readActive("sdk");
    if (!sdk.empty())
        setEnvironment("LUNA_SDK", installRoot("sdk", sdk).string());
}

int spawn(const std::vector<std::string>& arguments) {
    if (arguments.empty()) return kUsageError;
    std::vector<char*> native;
    native.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments)
        native.push_back(const_cast<char*>(argument.c_str()));
    native.push_back(nullptr);
#ifdef _WIN32
    const intptr_t status = _spawnvp(_P_WAIT, native[0], native.data());
    if (status == -1) {
        std::cerr << "lunax: cannot start '" << arguments.front()
                  << "': " << std::strerror(errno) << '\n';
        return 127;
    }
    return static_cast<int>(status);
#else
    const pid_t child = fork();
    if (child < 0) {
        std::cerr << "lunax: fork failed: " << std::strerror(errno) << '\n';
        return 127;
    }
    if (child == 0) {
        execvp(native[0], native.data());
        std::cerr << "lunax: cannot start '" << arguments.front()
                  << "': " << std::strerror(errno) << '\n';
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) continue;
        std::cerr << "lunax: waitpid failed: " << std::strerror(errno) << '\n';
        return 127;
    }
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 127;
#endif
}

int fetchAndExtract(const std::string& label, const std::string& url,
                    const fs::path& archive, const fs::path& target) {
    std::error_code error;
    if (fs::exists(target, error)) {
        std::cerr << "lunax: installation already exists: '" << target.string()
                  << "'\n";
        return kFilesystemError;
    }
    if (!fs::exists(archive, error)) {
        const int downloaded = downloadTo(url, archive);
        if (downloaded != 0) return downloaded;
    } else {
        std::cout << "lunax: using cached archive '" << archive.string()
                  << "'\n";
    }
    fs::create_directories(target, error);
    if (error) {
        reportFilesystem("cannot create installation", target, error);
        return kFilesystemError;
    }
    const int extracted = spawn({
        "tar", "-xf", archive.string(), "-C", target.string(),
        "--strip-components=1"});
    if (extracted != 0) {
        std::cerr << "lunax: archive extraction failed; incomplete files were retained at '"
                  << target.string() << "'\n";
        return kFilesystemError;
    }
    std::cout << "installed " << label << " at " << target.string() << '\n';
    return 0;
}

std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    std::string quoted = "\"";
    for (char byte : value) {
        if (byte == '\"') quoted += '\\';
        quoted += byte;
    }
    return quoted + "\"";
#else
    std::string quoted = "'";
    for (char byte : value)
        quoted += byte == '\'' ? "'\"'\"'" : std::string(1, byte);
    return quoted + "'";
#endif
}

void printExport(const std::string& name, const std::string& value) {
#ifdef _WIN32
    std::cout << "$env:" << name << " = " << shellQuote(value) << '\n';
#else
    std::cout << "export " << name << '=' << shellQuote(value) << '\n';
#endif
}

int listKind(const std::string& kind) {
    const fs::path directory = lunaxHome() / pluralKind(kind);
    const std::string active = readActive(kind);
    std::error_code error;
    if (!fs::exists(directory, error)) return 0;
    std::vector<std::string> versions;
    fs::directory_iterator iterator(directory, error);
    const fs::directory_iterator end;
    while (!error && iterator != end) {
        if (iterator->is_directory(error))
            versions.push_back(iterator->path().filename().string());
        iterator.increment(error);
    }
    if (error) {
        reportFilesystem("cannot list", directory, error);
        return kFilesystemError;
    }
    std::sort(versions.begin(), versions.end());
    for (const std::string& version : versions)
        std::cout << (version == active ? "* " : "  ") << kind << ' '
                  << version << '\n';
    return 0;
}

} // namespace

extern "C" {

int lunax_host_init(int32_t argc, char** argv) {
    if (argc < 0 || (argc != 0 && !argv)) return kUsageError;
    gArgc = argc;
    gArgv = argv;
    return 0;
}

int32_t lunax_host_arg_count() { return gArgc; }

const char* lunax_host_arg(int32_t index) {
    if (index < 0 || index >= gArgc || !gArgv || !gArgv[index]) return "";
    return gArgv[index];
}

int32_t lunax_host_text_equals(const char* left, const char* right) {
    return left && right && std::strcmp(left, right) == 0 ? 1 : 0;
}

void lunax_host_out_line(const char* text) {
    std::cout << (text ? text : "") << '\n';
}

void lunax_host_err_line(const char* text) {
    std::cerr << (text ? text : "") << '\n';
}

int32_t lunax_host_download(const char* url, const char* outputPath) {
    if (!url || !*url || !outputPath || !*outputPath) return kUsageError;
    return downloadTo(url, fs::u8path(outputPath));
}

int32_t lunax_host_install(const char* kindValue, const char* versionValue,
                           const char* urlValue) {
    const std::string kind = kindValue ? kindValue : "";
    const std::string version = versionValue ? versionValue : "";
    const std::string url = urlValue ? urlValue : "";
    if (!validKind(kind)) {
        std::cerr << "lunax: install kind must be compiler, toolchain, or sdk\n";
        return kUsageError;
    }
    if (!validVersion(version)) {
        std::cerr << "lunax: version may contain only letters, digits, '.', '_', and '-'\n";
        return kUsageError;
    }
    if (url.empty()) return kUsageError;

    const fs::path target = installRoot(kind, version);
    const fs::path archive = lunaxHome() / "downloads" /
        (kind + "-" + version + ".archive");
    const int installed = fetchAndExtract(
        kind + " " + version, url, archive, target);
    if (installed != 0) return installed;
    std::error_code error;
    if (kind == "compiler" &&
        !fs::is_regular_file(fs::u8path(executableName(target)), error)) {
        std::cerr << "lunax: compiler archive has no bin/luna executable\n";
        return kFilesystemError;
    }
    return 0;
}

int32_t lunax_host_list(const char* kindValue) {
    const std::string kind = kindValue ? kindValue : "";
    if (kind == "all") {
        int status = listKind("compiler");
        if (status == 0) status = listKind("toolchain");
        if (status == 0) status = listKind("sdk");
        return status;
    }
    if (!validKind(kind)) {
        std::cerr << "lunax: list kind must be compiler, toolchain, or sdk\n";
        return kUsageError;
    }
    return listKind(kind);
}

int32_t lunax_host_use(const char* kindValue, const char* versionValue) {
    const std::string kind = kindValue ? kindValue : "";
    const std::string version = versionValue ? versionValue : "";
    if (!validKind(kind) || !validVersion(version)) return kUsageError;
    const fs::path installation = installRoot(kind, version);
    std::error_code error;
    if (!fs::is_directory(installation, error)) {
        std::cerr << "lunax: " << kind << ' ' << version
                  << " is not installed\n";
        return kConfigurationError;
    }
    const fs::path destination = activeFile(kind);
    fs::create_directories(destination.parent_path(), error);
    if (error) {
        reportFilesystem("cannot create active configuration",
                         destination.parent_path(), error);
        return kFilesystemError;
    }
    std::ofstream output(destination, std::ios::trunc);
    output << version << '\n';
    if (!output) {
        std::cerr << "lunax: cannot update '" << destination.string()
                  << "'\n";
        return kFilesystemError;
    }
    std::cout << "active " << kind << " is now " << version << '\n';
    return 0;
}

int32_t lunax_host_package_fetch(const char* packageIdValue,
                                 const char* versionValue,
                                 const char* urlValue) {
    const std::string packageId = packageIdValue ? packageIdValue : "";
    const std::string version = versionValue ? versionValue : "";
    const std::string url = urlValue ? urlValue : "";
    if (!validPackageId(packageId)) {
        std::cerr << "lunax: package ID may contain only letters, digits, '.', '_', and '-'\n";
        return kUsageError;
    }
    if (!validVersion(version) || url.empty()) return kUsageError;
    const fs::path target = lunaxHome() / "packages" / packageId / version;
    const fs::path archive = lunaxHome() / "downloads" /
        ("package-" + packageId + "-" + version + ".archive");
    return fetchAndExtract(
        "package " + packageId + " " + version, url, archive, target);
}

int32_t lunax_host_package_list(const char* packageIdValue) {
    const std::string requested = packageIdValue ? packageIdValue : "";
    if (requested != "all" && !validPackageId(requested)) return kUsageError;
    const fs::path root = lunaxHome() / "packages";
    std::error_code error;
    if (!fs::exists(root, error)) return 0;
    std::vector<std::string> packages;
    fs::directory_iterator packageIterator(root, error);
    const fs::directory_iterator end;
    while (!error && packageIterator != end) {
        const std::string packageId =
            packageIterator->path().filename().string();
        if (packageIterator->is_directory(error) &&
            (requested == "all" || requested == packageId)) {
            fs::directory_iterator versionIterator(
                packageIterator->path(), error);
            while (!error && versionIterator != end) {
                if (versionIterator->is_directory(error))
                    packages.push_back(
                        packageId + " " +
                        versionIterator->path().filename().string());
                versionIterator.increment(error);
            }
        }
        packageIterator.increment(error);
    }
    if (error) {
        reportFilesystem("cannot list package cache", root, error);
        return kFilesystemError;
    }
    std::sort(packages.begin(), packages.end());
    for (const std::string& package : packages)
        std::cout << "  package " << package << '\n';
    return 0;
}

int32_t lunax_host_print_env() {
    const fs::path home = lunaxHome();
    printExport("LUNAX_HOME", home.string());
    printExport("LUNA_PACKAGE_HOME", (home / "packages").string());
    const std::string compiler = readActive("compiler");
    if (!compiler.empty()) {
        const fs::path root = installRoot("compiler", compiler);
        printExport("LUNA_HOME", root.string());
        printExport("LUNA_RUNTIME_LIB",
                    (root / "lib" / "libruntime.a").string());
    }
    const std::string toolchain = readActive("toolchain");
    if (!toolchain.empty())
        printExport("LUNA_TOOLCHAIN",
                    installRoot("toolchain", toolchain).string());
    const std::string sdk = readActive("sdk");
    if (!sdk.empty())
        printExport("LUNA_SDK", installRoot("sdk", sdk).string());
    return 0;
}

int32_t lunax_host_print_backend() {
    const SelectedBackend& backend = selectedBackend();
    if (!backend.descriptor) {
        std::cerr << "lunax: " << backend.error << '\n';
        return kConfigurationError;
    }
    std::cout << "download backend: " << backend.descriptor->name
              << " (ABI v" << backend.descriptor->abi_version << ")\n";
    std::cout << "selection: LUNAX_DOWNLOAD_BACKEND="
              << (environment("LUNAX_DOWNLOAD_BACKEND")
                      ? environment("LUNAX_DOWNLOAD_BACKEND") : "curl")
              << '\n';
    return 0;
}

int32_t lunax_host_print_compiler() {
    std::cout << selectedCompiler() << '\n';
    return 0;
}

int32_t lunax_host_forward_luna(int32_t firstArgument) {
    if (firstArgument < 0 || firstArgument > gArgc) return kUsageError;
    applyManagedEnvironment();
    std::vector<std::string> arguments;
    arguments.push_back(selectedCompiler());
    for (int32_t index = firstArgument; index < gArgc; ++index)
        arguments.emplace_back(gArgv[index] ? gArgv[index] : "");
    return spawn(arguments);
}

} // extern "C"
