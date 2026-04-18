# WebServer Docs

本文档目录基于当前工作树源码快照编写，目标不是重复 `README.md`，而是把这套代码：

- 现在是怎么工作的
- 做过哪些结构性修改
- 应该怎么从源码层阅读

讲清楚。

阅读顺序建议：

1. [architecture-guide.md](./architecture-guide.md)
2. [wiki-code-architecture.md](./wiki-code-architecture.md)
3. [change-notes.md](./change-notes.md)

文档说明：

- 为了兼容大多数 IDE 的 Markdown 预览，文档中的结构图优先使用本地图片和纯文本图。
- 所有说明都以当前仓库中的实际源码为准，而不是历史 README 或旧分支状态。

文档清单：

| 文件 | 内容定位 | 适合谁看 |
| --- | --- | --- |
| [architecture-guide.md](./architecture-guide.md) | 系统架构、线程模型、模块协作、数据流与设计取舍 | 想系统理解工程全貌的人 |
| [wiki-code-architecture.md](./wiki-code-architecture.md) | Wiki 风格源码导读，告诉你从哪开始看、按什么顺序看、重点看什么 | 新同学、第一次接手项目的人 |
| [change-notes.md](./change-notes.md) | 当前分支相对原始实现的重要结构修改、重构动机和后续建议 | 准备继续重构或做 review 的人 |

如果你的目标是：

- 快速搞懂“一个 HTTP 请求是怎么跑完的”，优先看 [wiki-code-architecture.md](./wiki-code-architecture.md)。
- 系统性理解这套 Reactor/TCP/HTTP 设计，优先看 [architecture-guide.md](./architecture-guide.md)。
- 了解这条 `optimize` 分支上有哪些关键抽象层已经落地，优先看 [change-notes.md](./change-notes.md)。
