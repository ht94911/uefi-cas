<p align="center"><img src="docs/assets/cas-banner.svg" alt="cas firmware — Xiaomi 10 Ultra" width="100%"></p>

<p align="center"><strong>English</strong> · <a href="README.zh-CN.md">简体中文</a></p>

# UEFI for Xiaomi 10 Ultra (cas)

The **Mu-Silicium firmware companion** to [UniXNU-cas](https://github.com/ht94911/UniXNU-cas), the project home for Xiaomi 10 Ultra / M2007J1SC / SM8250. Both repositories form one community device port.

The `cas-complete-001` candidate has **hardware-verified UEFI / UniXNU menus, physical buttons, read-only RAM FAT and EFI execution/return**, with three independent complete runs and a byte-identical 34-artifact rebuild. DarwinBoot / XNU / iOS have not been tested. Full results and evidence limits are maintained in the [project validation guide](https://github.com/ht94911/UniXNU-cas/blob/main/docs/validation.md).

## Firmware implementation

| Path | Purpose |
| --- | --- |
| [Platforms/Xiaomi/casPkg](Platforms/Xiaomi/casPkg) | SEC startup, board libraries, firmware-volume composition and cas drivers |
| [BootShim/CasBootShim.S](BootShim/CasBootShim.S) | Bounded RAM handoff from the stock bootloader |
| [BootShim/CasBootValidate.c](BootShim/CasBootValidate.c) | Input and memory checks, compiled with strict alignment |
| [Resources/Configs/cas.toml](Resources/Configs/cas.toml) | Fixed boot image and board parameters |
| [CasSmoke](Platforms/Xiaomi/casPkg/Applications/CasSmoke) | Identifiable EFI test application that returns to its caller |

The target preserves the ABL framebuffer, exposes GOP and physical keys, uses emulated variables and read-only RAM block I/O, and returns `EFI_UNSUPPORTED` for capsule requests. It has no physical storage controller or persistent-variable backend in the selected firmware. No vendor DXE blobs are selected by the cas build.

## Build and use

Use the cas-specific checked entry point with the sibling `UniXNU-cas` and pinned `UniXNU` source directories:

```sh
python3 "$CAS_WORK_ROOT/uefi-cas/Platforms/Xiaomi/casPkg/DeviceBuild.py" \
  --build --out "$CAS_WORK_ROOT/builds/cas-new-001"
```

Prepare the exact sources, submodules and environment using the [full build guide](https://github.com/ht94911/UniXNU-cas/blob/main/docs/build.md) first. The full boot image additionally needs the matching private stock DTB. The [public release](https://github.com/ht94911/UniXNU-cas/releases/tag/v0.1.0) provides reviewed source-built components and documentation; it does not distribute the complete device-bound boot image.

- [Temporary boot, controls and recovery](https://github.com/ht94911/UniXNU-cas/blob/main/docs/operations.md)
- [Hardware acceptance and reproducibility](https://github.com/ht94911/UniXNU-cas/blob/main/docs/validation.md)
- [Architecture and future operating-system work](https://github.com/ht94911/UniXNU-cas/blob/main/docs/architecture.md)
- [Contributing](CONTRIBUTING.md)

## Source identity

| Reference | Commit |
| --- | --- |
| Firmware base, Project-Silicium/Mu-Silicium | `42fe563791e51bf5500c2e470ed294ca0c94b97c` |
| Hardware-tested `cas-complete-001` firmware | `893f5025cea91a121f2566730779cb34955325fb` |
| Matching device tools | `346a404f6cee464e3e95f6ded464caf804a1f89e` |
| Matching UniXNU | `67e8ef0aa2a0a1fcb940889fb39ed1752b4b1593` |

The `cas-complete-001` tag preserves the tested commit. The `v0.1.0` publication adds docs/support files without changing firmware implementation. Boot image SHA-256 is `a085d19b6a4a1ea429f2f10cb191050f316cf044d4fd864b53d398e4f6a4f67c`; the complete image is kept locally.

## Upstream and licenses

This repository preserves the [Project Silicium](https://github.com/Project-Silicium/Mu-Silicium) history and notices. It builds on Microsoft Project Mu and existing Qualcomm/Kona support. UniXNU, sheng and pipa were sources/references as described in the [source guide](https://github.com/ht94911/UniXNU-cas/blob/main/docs/sources.md).

See [LICENSE](LICENSE), individual file headers and [the retained upstream README](docs/UPSTREAM-README.md). Original cas files use BSD-2-Clause-Patent where indicated; broader upstream directories can carry other licenses, including GPL. Generic upstream device/build documents describe the broader project; use the cas guide above for this port.
