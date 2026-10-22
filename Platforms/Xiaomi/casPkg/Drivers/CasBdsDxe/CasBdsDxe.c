// SPDX-License-Identifier: BSD-2-Clause-Patent
// Minimal source BDS for the cas console bring-up stage. Physical media absent.
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Guid/Fdt.h>
#include <Guid/FdtHob.h>
#include <Protocol/Bds.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/SimpleTextOut.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <CasPlatform.h>
#include <CasDisplay.h>

STATIC VOID EFIAPI EmptyNotify (EFI_EVENT Event, VOID *Context) { }

STATIC VOID ConnectFirmwareHandles (VOID) {
  EFI_HANDLE *Handles;
  UINTN Count, Index, Pass;
  // Three bounded passes allow graphics driver binding and console children.
  // The FDF contains no physical storage or USB drivers.
  for (Pass = 0; Pass < 3; ++Pass) {
    if (!EFI_ERROR (gBS->LocateHandleBuffer (AllHandles, NULL, NULL, &Count, &Handles))) {
      for (Index = 0; Index < Count; ++Index) {
        gBS->ConnectController (Handles[Index], NULL, NULL, TRUE);
      }
      FreePool (Handles);
    }
  }
}

STATIC EFI_STATUS SetConsole (EFI_GUID *Protocol, EFI_HANDLE *Handle, VOID **Interface) {
  EFI_HANDLE *Handles;
  UINTN Count;
  EFI_STATUS Status = gBS->LocateHandleBuffer (ByProtocol, Protocol, NULL, &Count, &Handles);
  if (EFI_ERROR (Status)) { return Status; }
  if (Count == 0) { FreePool (Handles); return EFI_NOT_FOUND; }
  *Handle = Handles[0];
  Status = gBS->HandleProtocol (*Handle, Protocol, Interface);
  FreePool (Handles);
  return Status;
}

STATIC VOID DrawMenu (UINTN Selected) {
  gST->ConOut->SetAttribute (gST->ConOut, EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  gST->ConOut->ClearScreen (gST->ConOut);
  Print (L"Xiaomi 10 Ultra (cas) UEFI bring-up\r\n\r\n");
  Print (L"%s Firmware information\r\n", Selected == 0 ? L">" : L" ");
  Print (L"%s Reboot to Android\r\n\r\n", Selected == 1 ? L">" : L" ");
  Print (L"Volume + / - : move    Power : select\r\n");
}

STATIC VOID EFIAPI BdsEntry (EFI_BDS_ARCH_PROTOCOL *This) {
  EFI_EVENT Event;
  EFI_HOB_GUID_TYPE *Hob;
  EFI_STATUS OutputStatus, InputStatus;
  EFI_INPUT_KEY Key;
  UINTN Selected = 0, Index;
  volatile CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  Info->Stage = CasStageBds;
  CasDisplayMark (CasStageBds);
  DEBUG ((DEBUG_WARN, "cas BDS entered\n"));
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if (Hob != NULL && GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (UINT64)) {
    UINT64 Address;
    CopyMem (&Address, GET_GUID_HOB_DATA (Hob), sizeof (Address));
    if (Address == CAS_FDT_BASE) {
      gBS->InstallConfigurationTable (&gFdtTableGuid, (VOID *)(UINTN)Address);
    }
  }
  if (!EFI_ERROR (gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_CALLBACK, EmptyNotify, NULL,
                                     &gEfiEndOfDxeEventGroupGuid, &Event))) {
    gBS->SignalEvent (Event);
    gBS->CloseEvent (Event);
  }
  ConnectFirmwareHandles ();
  OutputStatus = SetConsole (&gEfiSimpleTextOutProtocolGuid, &gST->ConsoleOutHandle, (VOID **)&gST->ConOut);
  InputStatus = SetConsole (&gEfiSimpleTextInProtocolGuid, &gST->ConsoleInHandle, (VOID **)&gST->ConIn);
  if (!EFI_ERROR (OutputStatus)) {
    gST->StandardErrorHandle = gST->ConsoleOutHandle;
    gST->StdErr = gST->ConOut;
  }
  gST->Hdr.CRC32 = 0;
  gBS->CalculateCrc32 (gST, gST->Hdr.HeaderSize, &gST->Hdr.CRC32);
  DEBUG ((DEBUG_WARN, "cas BDS console: output=%r input=%r\n", OutputStatus, InputStatus));
  if (EFI_ERROR (OutputStatus) || EFI_ERROR (InputStatus)) {
    if (!EFI_ERROR (OutputStatus)) { Print (L"Console driver missing: output=%r input=%r\r\n", OutputStatus, InputStatus); }
    for (;;) { gBS->Stall (100000); }
  }
  DrawMenu (Selected);
  for (;;) {
    if (EFI_ERROR (gBS->WaitForEvent (1, &gST->ConIn->WaitForKey, &Index)) ||
        EFI_ERROR (gST->ConIn->ReadKeyStroke (gST->ConIn, &Key))) { continue; }
    if (Key.ScanCode == SCAN_UP || Key.ScanCode == SCAN_DOWN) {
      Selected ^= 1;
      DrawMenu (Selected);
    } else if (Key.UnicodeChar == L'\r') {
      if (Selected == 1) {
        Print (L"Rebooting...\r\n");
        gRT->ResetSystem (EfiResetCold, EFI_SUCCESS, 0, NULL);
        Print (L"PSCI reset returned; use physical recovery.\r\n");
      } else {
        DrawMenu (Selected);
        Print (L"\r\nSM8250; FD=0x%Lx; bounded RAM heap=%u MiB\r\n", (UINT64)CAS_FD_BASE, CAS_HEAP_SIZE >> 20);
        Print (L"Entry EL%Lu; FDT=%Lu bytes; RAM payload=%Lu bytes\r\n", Info->EntryEl, Info->FdtSize, Info->InitrdSize);
        Print (L"Variables are emulated in RAM.\r\n");
      }
    }
  }
}

STATIC EFI_BDS_ARCH_PROTOCOL mBds = { BdsEntry };

EFI_STATUS EFIAPI CasBdsInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_HANDLE Handle = NULL;
  ((volatile CAS_BOOT_INFO *)(UINTN)CAS_LOG_BASE)->Stage = CasStageDxe;
  CasDisplayMark (CasStageDxe);
  DEBUG ((DEBUG_WARN, "cas DXE: BDS protocol installed\n"));
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBdsArchProtocolGuid, &mBds, NULL);
}
