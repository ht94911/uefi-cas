#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause-Patent
"""Explicit cas target entry. Uses pinned local BaseTools and mandatory preflight.

Unlike the generic Mu wrapper, this target does not fetch/update repositories,
patch every submodule or package the generic BootShim. Boot packaging is a
separate checked cas step.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[3]
PACKAGES_PATH = ("Platforms/Xiaomi", "Common/Mu", "Common/Mu_OEM_Sample", "Mu_Basecore", "Silicon/Qualcomm", "Silicon/Silicium")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    actions = ap.add_mutually_exclusive_group()
    actions.add_argument("--describe", action="store_true")
    actions.add_argument("--check", action="store_true")
    actions.add_argument("--genc", action="store_true")
    actions.add_argument("--modules", action="store_true")
    actions.add_argument("--build", action="store_true")
    ap.add_argument("--device", type=Path, default=Path(os.environ.get("CAS_DEVICE_ROOT", str(WORKSPACE.parent / "UniXNU-cas"))))
    ap.add_argument("--out", type=Path)
    args = ap.parse_args()
    if args.describe:
        print(json.dumps({"board": "cas", "soc": "SM8250", "soc_type": 0, "arch": "AARCH64",
                          "target": "DEBUG", "dsc": "casPkg/cas.dsc", "fd_base": "0x9f000000", "fd_size": "0x800000",
                          "workspace": str(WORKSPACE), "package_roots": PACKAGES_PATH, "vendor_components": []}, indent=2))
        return 0
    if args.genc or args.modules or args.build:
        if not args.out:
            ap.error("--genc/--modules/--build requires a new --out directory for preserved logs")
        command = [sys.executable, str(args.device / "scripts/build_firmware.py"), "--uefi", str(WORKSPACE),
                   "--out", str(args.out), "--phase", "genc" if args.genc else "modules" if args.modules else "build"]
    else:
        command = [sys.executable, str(args.device / "scripts/preflight.py"), "--uefi", str(WORKSPACE)]
    return subprocess.run(command).returncode


if __name__ == "__main__":
    sys.exit(main())
