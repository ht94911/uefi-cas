// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_XNU_DIAGNOSTICS
#error Diagnostic SerialPortLib must only be selected by cas-diag.dsc
#endif
#include <Library/BaseLib.h>
#include <Library/SerialPortLib.h>
#include <CasEarlyText.h>

// Keep the original bounded RAM logger and all other SerialPortLib functions.
#define SerialPortWrite CasRamLogWrite
#include "../SerialPortLib/SerialPortLib.c"
#undef SerialPortWrite

UINTN EFIAPI SerialPortWrite (UINT8 *Buffer, UINTN Count) {
  volatile CAS_BOOT_INFO *Info = (void *)(UINTN)CAS_LOG_BASE;
  CAS_EARLY_TEXT *T = (void *)(UINTN)CAS_EARLY_TEXT_ADDRESS;
  CAS_DISPLAY_STATE *S;
  CAS_FRAMEBUFFER *F;
  UINTN Written;
  BOOLEAN Interrupts = GetInterruptState ();
  DisableInterrupts ();
  Written = CasRamLogWrite (Buffer, Count);
  // The shared guard also blocks synchronous exception logging from recursing
  // through framebuffer access. This CPU0 diagnostic has no SMP writers.
  if (!Written || Info->Stage >= CasStageDxe) { goto Done; }
  if (!CasEarlyTextValid (T)) { CasEarlyTextReset (T); }
  if (*(volatile CAS_U32 *)&T->Busy || T->Frozen) { goto Done; }
  T->Busy = 1;
  MemoryFence ();
  CasEarlyTextFeed (T, Buffer, Written);
  S = CasDisplayState ();
  F = &S->Framebuffer;
  if (S->Magic == CAS_DISPLAY_MAGIC && S->Version == CAS_DISPLAY_VERSION &&
      S->Size == sizeof (*S) && S->Ready && !S->Stopped &&
      F->Base >= CAS_SPLASH_BASE && F->Bytes <= CAS_SPLASH_SIZE &&
      F->Base - CAS_SPLASH_BASE <= CAS_SPLASH_SIZE - F->Bytes &&
      CasEarlyTextRender (T, F) == 0) {
    if (T->Frames != MAX_UINT32) { ++T->Frames; }
    CasDisplayRefresh ();
  }
  MemoryFence ();
  T->Busy = 0;
Done:
  if (Interrupts) { EnableInterrupts (); }
  return Written;
}
