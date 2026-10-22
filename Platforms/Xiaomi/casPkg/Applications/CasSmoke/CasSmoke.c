// SPDX-License-Identifier: BSD-2-Clause-Patent
// No storage writes, variables, injected drivers, reset or ExitBootServices.
#include <Uefi.h>
#include <Protocol/LoadedImage.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

EFI_STATUS EFIAPI CasSmokeEntry (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_INPUT_KEY Key;
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;
  UINTN Index;
  STATIC CONST CHAR16 QemuArgument[] = L"cas-smoke:automated";
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  if (gST->ConOut == NULL) { return EFI_NOT_READY; }
  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"CAS EFI SMOKE - build cas-smoke-001\r\n");
  Print (L"StartImage reached the test application.\r\n");
  // An explicit test-runner argument is allowed only for offline QEMU checks.
  if (!EFI_ERROR (gBS->HandleProtocol (ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded)) &&
      Loaded->LoadOptions != NULL && Loaded->LoadOptionsSize == sizeof (QemuArgument) &&
      CompareMem (Loaded->LoadOptions, QemuArgument, sizeof (QemuArgument)) == 0) {
    Print (L"CAS_SMOKE_AUTOMATED_RETURN_SUCCESS\r\n"); return EFI_SUCCESS;
  }
  if (gST->ConIn == NULL) { Print (L"No input protocol; returning EFI_NOT_READY.\r\n"); return EFI_NOT_READY; }
  Print (L"Tap and release Power to return EFI_SUCCESS to UniXNU.\r\n");
  Print (L"Hold Power briefly (Escape) to return EFI_ABORTED.\r\n");
  for (;;) {
    Status = gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index);
    if (EFI_ERROR (Status)) { return Status; }
    if (EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) { continue; }
    if (Key.UnicodeChar == L'\r') { Print (L"CAS_SMOKE_RETURN_SUCCESS\r\n"); return EFI_SUCCESS; }
    if (Key.ScanCode == SCAN_ESC) { return EFI_ABORTED; }
  }
}
