// SPDX-License-Identifier: BSD-2-Clause-Patent
// Only the validated volatile prefix is cleared. Called after entry/FDT validation.
#include <CasPlatform.h>
.section .text.CasBootClearVolatileLog, "ax"
.balign 4
.global CasBootClearVolatileLog
.type CasBootClearVolatileLog, %function
CasBootClearVolatileLog:
  ldr x3, =CAS_LOG_BASE
  ldr x4, =CAS_BOOT_LOG_CLEAR_SIZE
1:
  stp xzr, xzr, [x3], #16
  subs x4, x4, #16
  b.ne 1b
  ret
.size CasBootClearVolatileLog, .-CasBootClearVolatileLog
