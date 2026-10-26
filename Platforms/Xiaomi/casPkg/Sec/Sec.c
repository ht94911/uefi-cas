/**
  cas SEC, adapted from the pinned Silicium SEC framework.
  Copyright (c) 2011-2017, ARM Limited. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/
#include <Uefi.h>
#include <PiPei.h>
#include <Guid/DxeMemoryProtectionSettings.h>
#include <Guid/FdtHob.h>
#include <Library/ArmLib.h>
#include <Library/ArmMmuLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/FdtLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PrePiLib.h>
#include <Library/PrePiHobListPointerLib.h>
#include <Library/SerialPortLib.h>
#include <CasPlatform.h>
#include <CasDisplay.h>
#include "Sec.h"

extern EFI_GUID gCasBootInfoGuid;

STATIC VOID Stop (EFI_STATUS Status) {
  volatile CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  Info->Error = Status;
  CasDisplayFault ((CAS_U64)Status);
  DEBUG ((DEBUG_ERROR, "cas SEC stopped: %r, stage=%u\n", Status, Info->Stage));
  CpuDeadLoop ();
}

#include <CasSharedInitrd.h>

VOID SecEntry (UINTN FdtAddress, UINTN InfoAddress) {
  EFI_STATUS Status;
  EFI_HOB_HANDOFF_INFO_TABLE *HobList;
  CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  DXE_MEMORY_PROTECTION_SETTINGS Protection;
  UINT64 Dtb;

  // The only supported entry is the validated cas shim; reject stale handoffs.
  if (InfoAddress != CAS_LOG_BASE || FdtAddress != CAS_FDT_BASE ||
      Info->Magic != CAS_BOOT_MAGIC || Info->Version != CAS_BOOT_VERSION ||
      Info->Size != sizeof (*Info) || Info->Stage != CasStageFdtCopied ||
      Info->FdtCopy != CAS_FDT_BASE || Info->FdtSize < 40 || Info->FdtSize > CAS_FDT_SIZE ||
      Info->InitrdCopy != CAS_PAYLOAD_BASE || Info->InitrdSize > CAS_PAYLOAD_SIZE) {
    CpuDeadLoop ();
  }
  ArmWriteVBar ((UINTN)SecVectorTable);
  Info->Stage = CasStageSec;
  CasDisplayMark (CasStageSec);
  SerialPortInitialize ();
  DEBUG ((DEBUG_WARN, "cas SEC: EL%Lu FDT=%Lx/%Lu, initrd=%Lx/%Lu\n",
          Info->EntryEl, Info->FdtCopy, Info->FdtSize, Info->OriginalInitrd, Info->InitrdSize));
  if (FdtCheckHeader ((VOID *)FdtAddress) != 0 || FdtTotalSize ((VOID *)FdtAddress) != Info->FdtSize) {
    Stop (EFI_COMPROMISED_DATA);
  }
#ifdef CAS_UPSTREAM_PROFILE
  if (!CasPrepareSharedInitrd ((VOID *)FdtAddress, Info)) { Stop (EFI_COMPROMISED_DATA); }
#endif
  // Preserve the physical/command-mode display and all SMMU state from ABL.
  // No framebuffer clear, old-XBL lookup, clock or Qualcomm platform init.
  if (ArmReadCntFrq () != CAS_TIMER_HZ) {
    DEBUG ((DEBUG_ERROR, "Unexpected CNTFRQ: %Lu\n", (UINT64)ArmReadCntFrq ()));
    Stop (EFI_UNSUPPORTED);
  }

  // D-cache is still off (enforced by the shim). Discard old destination lines
  // before writing RAM that will later become cached by MemoryPeim.
  InvalidateDataCacheRange ((VOID *)(UINTN)CAS_HEAP_BASE, CAS_HEAP_SIZE);
  if (Info->InitrdSize != 0) {
    InvalidateDataCacheRange ((VOID *)(UINTN)CAS_PAYLOAD_BASE, ALIGN_VALUE ((UINTN)Info->InitrdSize, 64));
    // EDK II CopyMem explicitly supports overlapping ranges.
    CopyMem ((VOID *)(UINTN)CAS_PAYLOAD_BASE, (VOID *)(UINTN)Info->OriginalInitrd, (UINTN)Info->InitrdSize);
  }

  HobList = HobConstructor ((VOID *)(UINTN)CAS_HEAP_BASE, CAS_HEAP_SIZE,
                           (VOID *)(UINTN)CAS_HEAP_BASE, (VOID *)(UINTN)(CAS_HEAP_BASE + CAS_HEAP_SIZE));
  PrePeiSetHobList (HobList);
  Status = MemoryPeim (CAS_HEAP_BASE, CAS_HEAP_SIZE);
  if (EFI_ERROR (Status)) { Stop (Status); }
  // HOB/allocator now exist. In particular, ArmMmuBaseLib's generated
  // constructor publishes its live-translation helper before permission edits.
  ProcessLibraryConstructorList ();
  // NC data needs an explicit execute-prohibit permission; the generic
  // Silicium *_XN aliases do not express it in their ARM memory attributes.
  Status = ArmSetMemoryAttributes (CAS_LOG_BASE, CAS_LOG_SIZE, EFI_MEMORY_XP, EFI_MEMORY_XP);
  if (EFI_ERROR (Status)) { Stop (Status); }
  Status = ArmSetMemoryAttributes (CAS_SPLASH_BASE, CAS_SPLASH_SIZE, EFI_MEMORY_XP, EFI_MEMORY_XP);
  if (EFI_ERROR (Status)) { Stop (Status); }
#ifdef CAS_XNU_DIAGNOSTICS
  // Only the diagnostic map includes this existing reserved region. Never add
  // it to conventional RAM. SEC installs RO/XP before entering any DXE module.
  Status = ArmSetMemoryAttributes (CAS_SMEM_BASE, CAS_SMEM_SIZE,
                                   EFI_MEMORY_RO | EFI_MEMORY_XP,
                                   EFI_MEMORY_RO | EFI_MEMORY_XP);
  if (EFI_ERROR (Status)) { Stop (Status); }
#endif
  Info->Stage = CasStageMmu;
  CasDisplayMark (CasStageMmu);
  BuildStackHob (CAS_STACK_BASE, CAS_STACK_SIZE);
  BuildCpuHob (ArmGetPhysicalAddressBits (), PcdGet8 (PcdPrePiCpuIoSize));
  SetBootMode (BOOT_WITH_DEFAULT_SETTINGS);
  BuildFvHob (PcdGet64 (PcdFvBaseAddress), PcdGet32 (PcdFvSize));
  // Mu does not enable a DXE policy without this HOB. Express the data/code
  // distinction explicitly; all image sections still get their own permissions.
  ZeroMem (&Protection, sizeof (Protection));
  Protection.StructVersion = DXE_MEMORY_PROTECTION_SETTINGS_CURRENT_VERSION;
  Protection.CpuStackGuard = TRUE;
  Protection.InstallMemoryAttributeProtocol = TRUE;
  Protection.ImageProtectionPolicy.Fields.ProtectImageFromUnknown = 1;
  Protection.ImageProtectionPolicy.Fields.ProtectImageFromFv = 1;
  Protection.ImageProtectionPolicy.Fields.RaiseErrorIfProtectionFails = 1;
  Protection.NxProtectionPolicy.Fields.EfiReservedMemoryType = 1;
  Protection.NxProtectionPolicy.Fields.EfiLoaderData = 1;
  Protection.NxProtectionPolicy.Fields.EfiBootServicesData = 1;
  Protection.NxProtectionPolicy.Fields.EfiRuntimeServicesData = 1;
  Protection.NxProtectionPolicy.Fields.EfiConventionalMemory = 1;
  Protection.NxProtectionPolicy.Fields.EfiUnusableMemory = 1;
  Protection.NxProtectionPolicy.Fields.EfiACPIReclaimMemory = 1;
  Protection.NxProtectionPolicy.Fields.EfiACPIMemoryNVS = 1;
  Protection.NxProtectionPolicy.Fields.EfiMemoryMappedIO = 1;
  Protection.NxProtectionPolicy.Fields.EfiMemoryMappedIOPortSpace = 1;
  if (BuildGuidDataHob (&gDxeMemoryProtectionSettingsGuid, &Protection, sizeof (Protection)) == NULL) {
    Stop (EFI_OUT_OF_RESOURCES);
  }
  Dtb = CAS_FDT_BASE;
  if (BuildGuidDataHob (&gFdtHobGuid, &Dtb, sizeof (Dtb)) == NULL ||
      BuildGuidDataHob (&gCasBootInfoGuid, Info, sizeof (*Info)) == NULL) {
    Stop (EFI_OUT_OF_RESOURCES);
  }
#ifdef CAS_UPSTREAM_PROFILE
  if (BuildGuidDataHob (&mUniXNUEntryArgHobGuid, &Dtb, sizeof (Dtb)) == NULL) { Stop (EFI_OUT_OF_RESOURCES); }
#endif
  Info->Stage = CasStageHobs;
  CasDisplayMark (CasStageHobs);
  DEBUG ((DEBUG_WARN, "cas SEC: MMU/HOB ready; decompressing DXE FV\n"));
  Status = DecompressFirstFv ();
  if (EFI_ERROR (Status)) { Stop (Status); }
  Info->Stage = CasStageDecompressed;
  CasDisplayMark (CasStageDecompressed);
  DEBUG ((DEBUG_WARN, "cas SEC: entering DXE core\n"));
  Status = LoadDxeCoreFromFv (NULL, 0);
  Stop (Status);
}
