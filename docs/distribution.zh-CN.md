# 分发策略

## Lunax 0.1.x 决策

Ubuntu 24.04 build 链接发行版提供的 `libcurl.so.4`。Lunax 只使用 HTTP/HTTPS easy API，
而发行版 libcurl 可能引入更多可选协议与认证依赖。直接把这套闭包复制进 archive 会扩大
安全和第三方许可证范围，却不会扩大已承诺的平台边界。

因此 Lunax 明确区分两类 package：

- MVP tar archive 仍是依赖系统库的 Ubuntu 24.04 x86_64 package；
- 下一个 Ubuntu release 同时生成原生 Debian package，由 `dpkg-shlibdeps` 计算共享库
  依赖，并显式依赖 `tar`。

CI 会构建 Debian package、检查 control metadata、要求存在 Ubuntu 24.04 的
`libcurl4t64` 与 `tar` 依赖、以非特权方式解包，并运行其中的 binary。不可变的 v0.1.0
release 不会被追溯添加 `.deb`。

## 延后的 portable archive

未来的跨发行版 archive 可以构建仅启用 HTTP/HTTPS 以及必要 TLS/证书功能的最小
libcurl。它必须固定源码 revision、关闭无关协议、记录完整依赖图、携带全部必要的
第三方 notice，并在没有系统 libcurl 的干净环境通过测试。在这些门禁齐备前，Lunax
不会声称 archive 是 self-contained。
