// SPDX-License-Identifier: BSD-2-Clause-Patent
#include "CasBootOption.h"
#include <Guid/GlobalVariable.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Protocol/LoadedImage.h>
#include <CasPlatform.h>
#include <CasDisplay.h>

EFI_STATUS CasBootUniXnu (EFI_HANDLE Parent) {
  EFI_STATUS Status;
  UINT16 Order;
  UINTN Size = sizeof (Order), Offset, PathSize;
  CHAR16 Name[9];
  UINT8 *Option;
  EFI_DEVICE_PATH_PROTOCOL *Path;
  EFI_LOADED_IMAGE_PROTOCOL *Loaded;
  EFI_HANDLE Image;
  STATIC CHAR16 Marker[] = L"unixnu:firmware";
  // A freshly initialized emulated store has exactly one registered option.
  Status = gRT->GetVariable (L"BootOrder", &gEfiGlobalVariableGuid, NULL, &Size, &Order);
  if (EFI_ERROR (Status) || Size != sizeof (Order)) { return EFI_NOT_FOUND; }
  UnicodeSPrint (Name, sizeof (Name), L"Boot%04x", Order);
  Size = 0;
  Status = gRT->GetVariable (Name, &gEfiGlobalVariableGuid, NULL, &Size, NULL);
  if (Status != EFI_BUFFER_TOO_SMALL || Size > 0x2000 || Size == 0) { return EFI_COMPROMISED_DATA; }
  Option = AllocatePool (Size);
  if (Option == NULL) { return EFI_OUT_OF_RESOURCES; }
  Status = gRT->GetVariable (Name, &gEfiGlobalVariableGuid, NULL, &Size, Option);
  if (EFI_ERROR (Status) || !CasParseUniXnuOption (Option, Size,
      (CONST EFI_GUID *)PcdGetPtr (PcdUniXNUApplicationFile), &Offset, &PathSize)) {
    FreePool (Option); return EFI_COMPROMISED_DATA;
  }
  Path = AllocateCopyPool (PathSize, Option + Offset);
  FreePool (Option);
  if (Path == NULL) { return EFI_OUT_OF_RESOURCES; }
  EfiSignalEventReadyToBoot ();
  Status = gBS->LoadImage (FALSE, Parent, Path, NULL, 0, &Image);
  FreePool (Path);
  if (EFI_ERROR (Status)) { return Status; }
  Status = gBS->HandleProtocol (Image, &gEfiLoadedImageProtocolGuid, (VOID **)&Loaded);
  if (!EFI_ERROR (Status)) {
    Loaded->LoadOptions = Marker;
    Loaded->LoadOptionsSize = sizeof (Marker);
    ((volatile CAS_BOOT_INFO *)(UINTN)CAS_LOG_BASE)->Stage = CasStageApplication;
    CasDisplayMark (CasStageApplication);
    DEBUG ((DEBUG_WARN, "cas BDS: starting registered UniXNU FV application\n"));
    Status = gBS->StartImage (Image, NULL, NULL);
    // StartImage unloads a returned UEFI application per the UEFI contract.
    // Do not touch its LoadedImage protocol or unload a stale handle.
  } else {
    gBS->UnloadImage (Image);
  }
  gBS->SetWatchdogTimer (0, 0, 0, NULL);
  DEBUG ((DEBUG_WARN, "cas BDS: UniXNU returned %r; recovery menu\n", Status));
  return Status;
}
