<p align="center"><img src="docs/assets/cas-banner.svg" alt="cas firmware — Xiaomi 10 Ultra" width="100%"></p>

<p align="center"><strong>English</strong> · <a href="README.zh-CN.md">简体中文</a></p>

# UEFI for Xiaomi 10 Ultra (cas)

The Mu-Silicium firmware companion to [UniXNU-cas](https://github.com/ht94911/UniXNU-cas), the UniXNU port for the Xiaomi 10 Ultra / M2007J1SC / SM8250.

## Status

- The `cas-complete-001` firmware is verified on hardware: UEFI and UniXNU menus, physical buttons, read-only RAM FAT, EFI execution and return, and the return to Android, over three complete runs.
- DarwinBoot hands over to XNU on hardware, and CPU0 reaches VM and early platform initialization.
- In progress: the standard firmware profile `casPkg/cas-upstream.dsc` with a static high-RAM window for the XNU kernel.

## Firmware implementation

| Path | Purpose |
| --- | --- |
| [Platforms/Xiaomi/casPkg](Platforms/Xiaomi/casPkg) | SEC startup, board libraries, firmware-volume composition and cas drivers |
| [BootShim/CasBootShim.S](BootShim/CasBootShim.S) | Bounded RAM handoff from the stock bootloader |
| [BootShim/CasBootValidate.c](BootShim/CasBootValidate.c) | Input and memory checks, compiled with strict alignment |
| [Resources/Configs/cas.toml](Resources/Configs/cas.toml) | Fixed boot image and board parameters |

The firmware keeps the ABL framebuffer, exposes GOP and the physical keys, uses emulated variables and read-only RAM block I/O, and returns `EFI_UNSUPPORTED` for capsule requests. It has no physical storage controller or persistent-variable backend. No vendor DXE blobs are selected.

## Build

The firmware is built through the UniXNU framework, using the device configuration in UniXNU-cas:

```sh
./unixnu build firmware -D ../UniXNU-cas --uefi ../uefi-cas
```

The complete boot image additionally needs the matching private stock DTB, which is not distributed.

## Source identity

| Reference | Commit |
| --- | --- |
| Firmware base, Project-Silicium/Mu-Silicium | `42fe563791e51bf5500c2e470ed294ca0c94b97c` |
| Hardware-tested `cas-complete-001` firmware | `893f5025cea91a121f2566730779cb34955325fb` |
| Matching device tools | `346a404f6cee464e3e95f6ded464caf804a1f89e` |

The `cas-complete-001` tag preserves the tested commit. Its boot image SHA-256 is `a085d19b6a4a1ea429f2f10cb191050f316cf044d4fd864b53d398e4f6a4f67c`.

## Upstream and licenses

This repository preserves the [Project Silicium](https://github.com/Project-Silicium/Mu-Silicium) history and notices. It builds on Microsoft Project Mu and the existing Qualcomm/Kona support, with UniXNU, UniXNU-sheng and UniXNU-umi as references.

See [LICENSE](LICENSE), the individual file headers and [the retained upstream README](docs/UPSTREAM-README.md). Original cas files use BSD-2-Clause-Patent where indicated; other upstream directories can carry other licenses, including GPL.
