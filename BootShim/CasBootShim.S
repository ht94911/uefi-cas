// SPDX-License-Identifier: BSD-2-Clause-Patent
// ARM64 Linux Image ABI; entered by ABL with x0 = FDT, MMU/D-cache off.
#include <CasPlatform.h>

.section .text.head, "ax"
.global _Head
_Head:
  b     _Start
  nop
  .quad CAS_TEXT_OFFSET
  .quad _ImageSize
  .quad 0
  .quad 0
  .quad 0
  .quad 0
  .ascii "ARM\x64"
  .long 0

_Start:
  msr   daifset, #0xf
  mov   x20, x0
  adr   x21, _Head
  ldr   x22, =_ImageSize
  ldr   x3, =CAS_KERNEL_BASE
  cmp   x21, x3
  b.lo  _Reject
  tst   x21, #0xfff
  b.ne  _Reject
  adds  x4, x21, x22
  b.cs  _Reject
  ldr   x3, =(CAS_KERNEL_BASE + CAS_KERNEL_SIZE)
  cmp   x4, x3
  b.hi  _Reject

  mrs   x24, CurrentEL
  cmp   x24, #4
  b.eq  1f
  cmp   x24, #8
  b.ne  _Reject
  mrs   x25, sctlr_el2
  b     2f
1: mrs   x25, sctlr_el1
2: tbnz  x25, #0, _Reject      // MMU on: do not assume identity mapping.
  tbnz  x25, #2, _Reject      // D-cache on: no unreviewed cache transition.
  tbnz  x25, #25, _Reject     // Big-endian entry is unsupported.

  adr   x3, _ScratchStackTop
  sub   sp, x3, #CAS_BOOT_INFO_SIZE
  mov   x0, x21
  mov   x1, x22
  mov   x2, x20
  mov   x3, x20
  mov   x4, sp
  bl    CasValidateBoot
  cbnz  w0, _Reject

  // Discard stale cache lines before filling the validated destination span.
  // The span comprises FD, FDT copy, SEC stack and log, with no foreign region.
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  ldr   x8, =CAS_FD_BASE
  ldr   x7, =(CAS_LOG_BASE + CAS_LOG_SIZE)
9: dc    ivac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  9b
  dsb   sy

  // The input tree and both incoming payloads are now validated. Until here,
  // only the scratch stack inside our own checked kernel input was written.
  bl    CasBootClearVolatileLog
  lsr   x24, x24, #2
  str   x24, [sp, #80]
  str   x25, [sp, #88]
#ifdef CAS_XNU_DIAGNOSTICS
  mrs   x5, cntvct_el0
  str   x5, [sp, #104]       // diagnostic boot stamp; constant across app exits
#endif
  mov   x3, sp
  ldr   x4, =CAS_LOG_BASE
  mov   x5, #CAS_BOOT_INFO_SIZE
4: ldp   x6, x7, [x3], #16
  stp   x6, x7, [x4], #16
  subs  x5, x5, #16
  b.ne  4b

  // Framebuffer writes are optional and occur only after memory validation.
  // The shared cas decoder requires a real, bounded linear display route.
  bl    CasDisplayInitialize
  mov   w0, #1
  bl    CasDisplayMark

#ifdef CAS_TRACE_RECOVERY_ONLY
  // No FD copy, SEC/DXE/ArmCpuDxe, boot services, or XNU execution.
  bl    CasTraceRecoverEarly
10: wfe
  b     10b
#endif

  adr   x3, _Payload
  ldr   x4, =CAS_FD_BASE
  ldr   x5, =CAS_FD_SIZE
5: ldp   x6, x7, [x3], #16
  stp   x6, x7, [x4], #16
  subs  x5, x5, #16
  b.ne  5b
  ldr   x8, =CAS_LOG_BASE
  mov   w9, #2
  str   w9, [x8, #12]
  mov   w0, #2
  bl    CasDisplayMark

  mov   x3, x20
  ldr   x4, =CAS_FDT_BASE
  ldr   x8, =CAS_LOG_BASE
  ldr   x5, [x8, #40]
6: ldrb  w6, [x3], #1
  strb  w6, [x4], #1
  subs  x5, x5, #1
  b.ne  6b
  mov   w9, #3
  str   w9, [x8, #12]
  mov   w0, #3
  bl    CasDisplayMark

  dsb   sy
  // Publish instructions across possible cache aliases left by the loader.
  mrs   x9, ctr_el0
  ubfx  x9, x9, #16, #4
  mov   x10, #4
  lsl   x9, x10, x9
  ldr   x8, =CAS_FD_BASE
  ldr   x7, =(CAS_FD_BASE + CAS_FD_SIZE)
7: dc    cvac, x8
  add   x8, x8, x9
  cmp   x8, x7
  b.lo  7b
  dsb   sy
  ic    iallu
  dsb   sy
  isb
  ldr   x0, =CAS_FDT_BASE
  ldr   x1, =CAS_LOG_BASE
  ldr   x5, =CAS_FD_BASE
  br    x5

_Reject:
  // No firmware destination or device register has been written on rejection.
  // PSCI SYSTEM_RESET returns to the ordinary boot chain if supported.
  movz  x0, #9
  movk  x0, #0x8400, lsl #16
  mov   x1, xzr
  mov   x2, xzr
  mov   x3, xzr
  smc   #0
8: wfe
  b     8b
  .ltorg
