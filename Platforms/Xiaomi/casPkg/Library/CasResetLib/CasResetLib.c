// SPDX-License-Identifier: BSD-2-Clause-Patent
// Explicit Mu HwResetSystemLib binding. No restart-reason or PMIC writes.
#include <Uefi.h>
#include <IndustryStandard/ArmStdSmc.h>
#include <Library/ArmMonitorLib.h>
#include <Library/DebugLib.h>
#include <Library/ResetSystemLib.h>
#include <Library/UefiRuntimeLib.h>
#include <CasDisplay.h>

STATIC VOID RequestReset (UINTN Function) {
  ARM_MONITOR_ARGS Args = {0};
  Args.Arg0 = Function;
  ArmMonitorCall (&Args);
  // A successful reset/off call never returns. Avoid boot-service memory or
  // display accesses after ExitBootServices if the firmware rejected the call.
  if (!EfiAtRuntime ()) {
    CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
    Info->Error = EFI_DEVICE_ERROR; Info->Reserved[2] = Args.Arg0;
    DEBUG ((DEBUG_ERROR, "cas PSCI request %Lx returned %Lx; physical recovery required\n", (UINT64)Function, (UINT64)Args.Arg0));
    CasDisplayFault (0x5253540000000000ULL | (Args.Arg0 & 0xffffffff));
  }
}

VOID EFIAPI ResetCold (VOID) { RequestReset (ARM_SMC_ID_PSCI_SYSTEM_RESET); }
VOID EFIAPI ResetWarm (VOID) { ResetCold (); }
VOID EFIAPI ResetShutdown (VOID) { RequestReset (ARM_SMC_ID_PSCI_SYSTEM_OFF); }
VOID EFIAPI ResetPlatformSpecific (UINTN DataSize, VOID *ResetData) { ResetCold (); }
VOID EFIAPI ResetSystem (EFI_RESET_TYPE Type, EFI_STATUS Status, UINTN Size, VOID *Data OPTIONAL) {
  if (Type == EfiResetShutdown) { ResetShutdown (); }
  else if (Type == EfiResetWarm || Type == EfiResetCold || Type == EfiResetPlatformSpecific) { ResetCold (); }
}
