# Lunax

Lunax 是 Luna 生态管理工具，用于安装和选择 Luna 编译器、工具链与 SDK，维护传递给
Luna 的环境变量，通过可替换后端下载产物，并把自身未占用的 CLI 命令原样转发给当前
选中的 Luna 编译器。

这是一个明确标注边界的 Luna 0.2 MVP。命令分发器使用 Luna 编写；由于当前 Std 尚不能
安全表达 argv 索引、进程启动、owned path、动态库和 libcurl variadic 调用，仓库暂时包含
一个很窄的 native host library。随着 Luna 0.3 和标准库成熟，该兼容层应逐步删除。

## 构建

需要 CMake 3.20+、C++17 编译器、libcurl 开发文件、`tar`，以及当前 Luna 编译器和
Runtime：

```sh
cmake -S . -B build \
  -DLUNA_COMPILER=/path/to/luna \
  -DLUNA_RUNTIME_LIB=/path/to/libruntime.a
cmake --build build
ctest --test-dir build --output-on-failure
```

生成的程序位于 `build/luna-src/org.luna.lunax`。

## 命令

```text
lunax help
lunax version
lunax env
lunax backend
lunax which
lunax download <url> <output>
lunax install <compiler|toolchain|sdk> <version> <url>
lunax list [compiler|toolchain|sdk]
lunax use <compiler|toolchain|sdk> <version>
lunax package fetch <package-id> <version> <url>
lunax package list [package-id]
lunax luna <Luna 参数...>
lunax <任意 Luna CLI 命令...>
```

例如 `lunax check app` 与 `lunax build app -O2` 会原样转发。可用
`LUNAX_LUNA=/path/to/luna` 指定未纳管的默认编译器。

## 环境变量

Lunax 使用 `LUNAX_HOME`、`LUNAX_LUNA`、`LUNAX_DOWNLOAD_BACKEND` 和
`LUNAX_BACKEND_PATH` 进行配置；它会为转发的编译器进程维护 `LUNA_HOME`、
`LUNA_RUNTIME_LIB`、`LUNA_TOOLCHAIN`、`LUNA_SDK` 和 `LUNA_PACKAGE_HOME`。
`lunax env` 可输出供交互 shell 使用的赋值语句。

初始 installer 接受只有一个顶层目录的 tar-compatible archive。`package fetch` 会把
reverse-DNS package archive 放入按版本组织的本地 cache；当前 Luna 编译器还不会把它
解析为 registry。Lunax 目前尚未实现 checksum/signature 校验、包依赖求解、partial install
修复或删除操作，因此它首先是一个真正可运行的
集成实战项目，还不是面向安全敏感生产环境的包管理器。参见
[架构说明](docs/architecture.md)与[后端 ABI](docs/backend-plugin.md)。
