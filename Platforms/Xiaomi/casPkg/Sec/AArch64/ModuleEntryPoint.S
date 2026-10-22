// Copyright (c) 2011-2025, Arm Limited. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <AsmMacroLib.h>
#include <Library/ArmLib.h>
#include <CasPlatform.h>

// Deliberately use one COFF .text section for this entry, without patching the
// shared MdePkg AsmMacroLib for every module in the workspace.
.text
.align 4
.globl _ModuleEntryPoint
_ModuleEntryPoint:
  msr   daifset, #0xf
  mov   x20, x0
  mov   x21, x1
  // All firmware destinations were validated before the shim branched here.
  MOV64 (x0, CAS_STACK_BASE)
  MOV32 (x1, CAS_STACK_SIZE)
  add   x2, x0, x1
  mov   sp, x2
0: stp   xzr, xzr, [x0], #16
  cmp   x0, x2
  b.lo  0b
  bl    ASM_PFX (ArmEnableVFP)
  EL1_OR_EL2 (x0)
1: bl    ASM_PFX (SetupExceptionLevel1)
  b     3f
2: bl    ASM_PFX (SetupExceptionLevel2)
3: mov   x0, x20
  mov   x1, x21
  mov   x29, xzr
  bl    ASM_PFX (SecEntry)
4: wfe
  b     4b

.globl CasReadExceptionRegisters
CasReadExceptionRegisters:
  mrs   x2, CurrentEL
  cmp   x2, #8
  b.eq  5f
  mrs   x3, esr_el1
  mrs   x4, far_el1
  b     6f
5: mrs   x3, esr_el2
  mrs   x4, far_el2
6: str   x3, [x0]
  str   x4, [x1]
  ret
