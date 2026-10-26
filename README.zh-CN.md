<p align="center"><img src="docs/assets/cas-banner.svg" alt="cas 固件 — 小米 10 Ultra" width="100%"></p>

<p align="center"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 小米 10 Ultra（cas）UEFI

本仓库是 [UniXNU-cas](https://github.com/ht94911/UniXNU-cas) 的 Mu-Silicium 固件配套仓库。UniXNU-cas 是小米 10 Ultra / M2007J1SC / SM8250 的 UniXNU 适配。

## 状态

- `cas-complete-001` 固件已在真机验证：UEFI 与 UniXNU 菜单、实体按键、只读 RAM FAT、EFI 运行与返回、回 Android，完成三轮完整运行。
- DarwinBoot 已在真机把控制权交给 XNU，CPU0 已到 VM 与早期平台初始化。
- 进行中：标准固件配置 `casPkg/cas-upstream.dsc`，为 XNU 内核提供静态高内存窗口。

## 固件实现

| 路径 | 用途 |
| --- | --- |
| [Platforms/Xiaomi/casPkg](Platforms/Xiaomi/casPkg) | SEC 启动、板级库、固件卷组成和 cas 驱动 |
| [BootShim/CasBootShim.S](BootShim/CasBootShim.S) | 从原引导器接收输入的有界 RAM 交接 |
| [BootShim/CasBootValidate.c](BootShim/CasBootValidate.c) | 采用严格对齐编译的输入和内存检查 |
| [Resources/Configs/cas.toml](Resources/Configs/cas.toml) | 固定的启动镜像和板级参数 |

固件保留 ABL framebuffer，提供 GOP 与实体按键，使用模拟变量和只读 RAM Block I/O，Capsule 请求返回 `EFI_UNSUPPORTED`。固件不含物理存储控制器或持久变量后端，也不选用厂商 DXE blob。

## 构建

固件通过 UniXNU 框架构建，使用 UniXNU-cas 中的设备配置：

```sh
./unixnu build firmware -D ../UniXNU-cas --uefi ../uefi-cas
```

完整启动镜像还需要匹配的私有原厂 DTB，不随仓库分发。

## 源码身份

| 引用 | 提交 |
| --- | --- |
| 固件基础 Project-Silicium/Mu-Silicium | `42fe563791e51bf5500c2e470ed294ca0c94b97c` |
| 真机验收 `cas-complete-001` 固件 | `893f5025cea91a121f2566730779cb34955325fb` |
| 对应设备工具 | `346a404f6cee464e3e95f6ded464caf804a1f89e` |

`cas-complete-001` 标签保留真机验收提交，其启动镜像 SHA-256 为 `a085d19b6a4a1ea429f2f10cb191050f316cf044d4fd864b53d398e4f6a4f67c`。

## 上游与许可证

本仓库保留 [Project Silicium](https://github.com/Project-Silicium/Mu-Silicium) 的历史和许可，基于 Microsoft Project Mu 及已有的 Qualcomm/Kona 支持，参考了 UniXNU、UniXNU-sheng 与 UniXNU-umi。

许可见 [LICENSE](LICENSE)、各文件头和[保留的上游 README](docs/UPSTREAM-README.md)。标明的 cas 原创文件采用 BSD-2-Clause-Patent，其他上游目录可能使用包括 GPL 在内的不同许可证。
