# WebServer Docs

本文档目录基于当前仓库源码快照编写，重点覆盖三件事：

1. 解释项目当前的实际架构与运行链路。
2. 用源码视角拆解关键类、关键方法和线程模型。
3. 记录当前代码中值得继续演进的重构方向。

说明：

- 为了兼容大多数 IDE 的 Markdown 预览器，文档里的结构图统一使用纯文本/ASCII 图，而不是 Mermaid。
- 这样即使没有额外插件，文档也可以稳定预览。

推荐阅读顺序：

1. [architecture-guide.md](./architecture-guide.md)
2. [wiki-code-architecture.md](./wiki-code-architecture.md)
3. [change-notes.md](./change-notes.md)

文档清单：

| 文件 | 作用 | 适合谁看 |
| --- | --- | --- |
| [architecture-guide.md](./architecture-guide.md) | 系统架构说明，覆盖模块关系、线程模型、请求链路、数据流与设计取舍 | 第一次接手项目、准备做大改造的人 |
| [wiki-code-architecture.md](./wiki-code-architecture.md) | Wiki 风格源码导读，告诉你“从哪里开始看”“按什么顺序看”“怎么带着问题看” | 新同学、需要快速进入状态的人 |
| [change-notes.md](./change-notes.md) | 基于当前源码的修改说明与后续重构建议 | 准备继续重构、整理宏配置、收敛策略层的人 |

阅读建议：

- 如果你只想快速理解“浏览器访问 `/hello` 时代码怎么跑”，先看 [wiki-code-architecture.md](./wiki-code-architecture.md)。
- 如果你要做结构重构或性能优化，先看 [architecture-guide.md](./architecture-guide.md)，再看 [change-notes.md](./change-notes.md)。
- 如果你准备整理编译期宏、降低业务层对宏的感知，重点看 [change-notes.md](./change-notes.md)。
