// Copyright (c) 2012-2013, ARM Limited. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <PiPei.h>
#include <Library/ArmLib.h>
#include <Library/PrintLib.h>
#include <Library/SerialPortLib.h>
#include <CasPlatform.h>
#include <CasDisplay.h>
#include "Sec.h"

VOID CasReadExceptionRegisters (UINT64 *Esr, UINT64 *Far);

VOID SecCommonExceptionEntry (UINT32 Type, UINTN Lr) {
  CHAR8 Buffer[180];
  UINT64 Esr, Far;
  UINTN Length;
  volatile CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  CasReadExceptionRegisters (&Esr, &Far);
  Info->Error = Esr;
  Info->Reserved[0] = Lr;
  Info->Reserved[1] = Far;
  CasDisplayFault (Esr);
  Length = AsciiSPrint (Buffer, sizeof (Buffer), "cas exception: type=%u ELR=%Lx ESR=%Lx FAR=%Lx stage=%u\n",
                       Type, (UINT64)Lr, Esr, Far, Info->Stage);
  SerialPortWrite ((UINT8 *)Buffer, Length);
  CpuDeadLoop ();
}
