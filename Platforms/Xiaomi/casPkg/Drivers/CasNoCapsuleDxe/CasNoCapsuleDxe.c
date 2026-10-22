// SPDX-License-Identifier: BSD-2-Clause-Patent
// Satisfy the PI architecture contract while supporting no capsule formats.
#include <Uefi.h>
#include <Protocol/Capsule.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

STATIC EFI_STATUS EFIAPI RejectUpdateCapsule (
  IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
  IN UINTN CapsuleCount,
  IN EFI_PHYSICAL_ADDRESS ScatterGatherList
  )
{
  // Do not inspect caller buffers, write variables or schedule an update/reset.
  return EFI_UNSUPPORTED;
}

STATIC EFI_STATUS EFIAPI RejectQueryCapsuleCapabilities (
  IN EFI_CAPSULE_HEADER **CapsuleHeaderArray,
  IN UINTN CapsuleCount,
  OUT UINT64 *MaximumCapsuleSize,
  OUT EFI_RESET_TYPE *ResetType
  )
{
  // Output arguments have no defined result on EFI_UNSUPPORTED.
  return EFI_UNSUPPORTED;
}

EFI_STATUS EFIAPI CasNoCapsuleInitialize (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  EFI_UPDATE_CAPSULE PreviousUpdate = gRT->UpdateCapsule;
  EFI_QUERY_CAPSULE_CAPABILITIES PreviousQuery = gRT->QueryCapsuleCapabilities;

  gRT->UpdateCapsule = RejectUpdateCapsule;
  gRT->QueryCapsuleCapabilities = RejectQueryCapsuleCapabilities;
  // PI requires both runtime pointers before this NULL-interface notification.
  // DXE Core's architectural notification recalculates the runtime-table CRC.
  Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle, &gEfiCapsuleArchProtocolGuid, NULL, NULL);
  if (EFI_ERROR (Status)) {
    // A failed entry can be unloaded; never leave pointers into that image.
    gRT->UpdateCapsule = PreviousUpdate;
    gRT->QueryCapsuleCapabilities = PreviousQuery;
  }
  return Status;
}
