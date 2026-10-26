# Current UniXNU format2 firmware profile

cas-upstream.dsc/fdf is a new profile; cas.dsc and cas-diag.dsc remain reproducible
historical profiles. The active profile includes the standard UniXNU application,
UniXNUDxe, guarded DarwinBoot, UniXNURamDiskDxe, DiskIo, UnicodeCollation and FAT.
Its output directory is Build/casUpstream001, separate from accepted candidates.

SEC publishes the standard entry-argument HOB after validating the cas shim
handoff. Only the copied FDT's existing initrd cells are rewritten to the already
bounded FAT copy at0xa4000000 (maximum64MiB); property widths and total size stay
fixed. The shared driver remains read-only. Firmware uses the exact Mu_Basecore
recorded by this UEFI checkout, never a different framework EDK2 header set.

The loader module is staged by the device preflight from reviewed sources, actual
kernel/ADT hashes and a new candidate contract. The profile's forced-include
requires the generated guard marker; the plain upstream loader cannot silently
replace fresh facts, exact allocation or the retained full-range readback gate.
Missing private inputs refuse the build. Actual firmware compilation and device
execution must be recorded separately; profile source is not a hardware pass.

本配置保留cas固定内存、只读RAM盘与恢复边界。标准RAM盘的HOB只指向已验证的
BootShim副本；loader仍执行当前事实、精确分配和同一分配内读回，不能无保护启动。
