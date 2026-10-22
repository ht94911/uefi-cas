<p align="center"><img src="docs/assets/cas-banner.svg" alt="cas 固件 — 小米 10 Ultra" width="100%"></p>

<p align="center"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 小米 10 Ultra（cas）UEFI

本仓库是 [UniXNU-cas](https://github.com/ht94911/UniXNU-cas) 的 **Mu-Silicium 固件配套仓库**。项目面向小米 10 Ultra / M2007J1SC / SM8250，两个仓库共同组成一个社区设备适配，统一入口在 `UniXNU-cas`。

`cas-complete-001` 已在真机验证 **UEFI／UniXNU 菜单、实体按键、只读 RAM FAT 及 EFI 运行／返回**，完成三轮独立完整运行和 34 项产物逐字节一致重建。DarwinBoot／XNU／iOS 尚未测试，完整结果与证据限制见[项目验收手册](https://github.com/ht94911/UniXNU-cas/blob/main/docs/validation.zh-CN.md)。

## 固件实现

| 路径 | 用途 |
| --- | --- |
| [Platforms/Xiaomi/casPkg](Platforms/Xiaomi/casPkg) | SEC 启动、板级库、固件卷组成和 cas 驱动 |
| [BootShim/CasBootShim.S](BootShim/CasBootShim.S) | 从原引导器接收输入的有界 RAM 交接 |
| [BootShim/CasBootValidate.c](BootShim/CasBootValidate.c) | 采用严格对齐编译的输入和内存检查 |
| [Resources/Configs/cas.toml](Resources/Configs/cas.toml) | 固定启动镜像和板级参数 |
| [CasSmoke](Platforms/Xiaomi/casPkg/Applications/CasSmoke) | 有可识别标记、能够返回调用者的 EFI 测试程序 |

目标保留 ABL framebuffer，提供 GOP 与实体按键，使用模拟变量和只读 RAM Block I/O，Capsule 请求返回 `EFI_UNSUPPORTED`。选中的固件不含物理存储控制器或持久变量后端，cas 构建不选用厂商 DXE blob。

## 构建与操作

先准备相邻的 `UniXNU-cas` 和固定 `UniXNU` 源码目录，再使用 cas 专用校验入口：

```sh
python3 "$CAS_WORK_ROOT/uefi-cas/Platforms/Xiaomi/casPkg/DeviceBuild.py" \
  --build --out "$CAS_WORK_ROOT/builds/cas-new-001"
```

准确源码、子模块和环境准备步骤见[完整构建说明](https://github.com/ht94911/UniXNU-cas/blob/main/docs/build.zh-CN.md)。完整启动镜像还需要匹配的私有原厂 DTB。[公开发布](https://github.com/ht94911/UniXNU-cas/releases/tag/v0.1.0)提供审阅过的源码构建组件和文档，不分发完整设备绑定启动镜像。

- [临时启动、按键与恢复](https://github.com/ht94911/UniXNU-cas/blob/main/docs/operations.zh-CN.md)
- [真机验收与可复现记录](https://github.com/ht94911/UniXNU-cas/blob/main/docs/validation.zh-CN.md)
- [架构与后续系统移植](https://github.com/ht94911/UniXNU-cas/blob/main/docs/architecture.zh-CN.md)
- [贡献说明](CONTRIBUTING.zh-CN.md)

## 源码身份

| 引用 | 提交 |
| --- | --- |
| 固件基础 Project-Silicium/Mu-Silicium | `42fe563791e51bf5500c2e470ed294ca0c94b97c` |
| 真机验收 `cas-complete-001` 固件 | `893f5025cea91a121f2566730779cb34955325fb` |
| 对应设备工具 | `346a404f6cee464e3e95f6ded464caf804a1f89e` |
| 对应 UniXNU | `67e8ef0aa2a0a1fcb940889fb39ed1752b4b1593` |

`cas-complete-001` 标签保留真机验收提交，`v0.1.0` 增加文档／支持文件，固件实现未改。启动镜像 SHA-256 为 `a085d19b6a4a1ea429f2f10cb191050f316cf044d4fd864b53d398e4f6a4f67c`，完整镜像保留在本地。

## 上游与许可证

本仓库保留 [Project Silicium](https://github.com/Project-Silicium/Mu-Silicium) 的历史和许可，基于 Microsoft Project Mu 及已有 Qualcomm／Kona 支持。UniXNU、sheng 与 pipa 的来源／参考关系见[来源说明](https://github.com/ht94911/UniXNU-cas/blob/main/docs/sources.zh-CN.md)。

许可见 [LICENSE](LICENSE)、具体文件头和[保留的上游 README](docs/UPSTREAM-README.md)。标明的 cas 原创文件采用 BSD-2-Clause-Patent，其他上游目录可使用包括 GPL 在内的不同许可证。仓库原有通用设备／构建文档面向更广泛的上游项目，本适配请使用上方 cas 手册。
