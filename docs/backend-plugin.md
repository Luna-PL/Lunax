# Download backend ABI v1

Dynamic backends include `lunax/backend_abi.h` and export exactly:

```c
const LunaxDownloadBackendV1* lunax_download_backend_v1(void);
```

The descriptor and `context` remain valid for the process lifetime. The callback must:

- consume only fields covered by `request->struct_size`;
- reject an unsupported `abi_version`;
- write only to a new destination when `LUNAX_DOWNLOAD_REJECT_OVERWRITE` is set;
- verify certificates and hostnames when TLS verification is required;
- keep diagnostics in the caller-provided buffer and always NUL-terminate a non-empty buffer;
- return a stable `LunaxDownloadStatusV1` value;
- never release, rename, or delete resources owned by Lunax.

The ABI is backend-neutral. A wget backend is intentionally not part of the 0.1 repository
and is deferred until after Luna 0.3; it will be a separately distributed shared library.
The build contains a non-installed `fake-dynamic` test module solely to exercise descriptor
validation and dynamic loading in CI.
