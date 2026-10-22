// SPDX-License-Identifier: BSD-2-Clause-Patent
// A read-only view of the bounded payload already copied by the cas BootShim.
#include <Uefi.h>
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <CasPlatform.h>

STATIC UINT8 *mBase;
STATIC EFI_BLOCK_IO_MEDIA mMedia;
STATIC struct {
  VENDOR_DEVICE_PATH Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} mPath = {
  { { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
    { 0xafbb82e9, 0x32d6, 0x4bfa, { 0x86, 0x1c, 0xa3, 0x05, 0xc6, 0x9f, 0xb0, 0x71 } } },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { 4, 0 } }
};

BOOLEAN CasRamDiskInfoValid (CONST CAS_BOOT_INFO *Info) {
  return Info != NULL && Info->Magic == CAS_BOOT_MAGIC && Info->Version == CAS_BOOT_VERSION &&
         Info->Size == sizeof (*Info) && Info->InitrdCopy == CAS_PAYLOAD_BASE &&
         Info->InitrdSize >= 512 && Info->InitrdSize <= CAS_PAYLOAD_SIZE && (Info->InitrdSize % 512) == 0;
}
STATIC EFI_STATUS EFIAPI Reset (EFI_BLOCK_IO_PROTOCOL *This, BOOLEAN Extended) { return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Flush (EFI_BLOCK_IO_PROTOCOL *This) { return EFI_SUCCESS; }
STATIC EFI_STATUS EFIAPI Read (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba,
                               UINTN Size, VOID *Buffer) {
  UINT64 Blocks;
  if (MediaId != mMedia.MediaId) { return EFI_MEDIA_CHANGED; }
  if (Size == 0) { return EFI_SUCCESS; }
  if (Buffer == NULL) { return EFI_INVALID_PARAMETER; }
  if (Size % 512 != 0) { return EFI_BAD_BUFFER_SIZE; }
  Blocks = Size / 512;
  if (Lba > mMedia.LastBlock || Blocks > mMedia.LastBlock - Lba + 1) { return EFI_INVALID_PARAMETER; }
  CopyMem (Buffer, mBase + (UINTN)Lba * 512, Size);
  return EFI_SUCCESS;
}
STATIC EFI_STATUS EFIAPI Write (EFI_BLOCK_IO_PROTOCOL *This, UINT32 MediaId, EFI_LBA Lba,
                                UINTN Size, VOID *Buffer) {
  return MediaId == mMedia.MediaId ? EFI_WRITE_PROTECTED : EFI_MEDIA_CHANGED;
}
STATIC EFI_BLOCK_IO_PROTOCOL mBlock = { EFI_BLOCK_IO_PROTOCOL_REVISION, &mMedia, Reset, Read, Write, Flush };

EFI_STATUS EFIAPI CasRamDiskInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  CONST CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  EFI_HANDLE Handle = NULL;
  if (!CasRamDiskInfoValid (Info)) { return EFI_NOT_FOUND; }
  mBase = (VOID *)(UINTN)CAS_PAYLOAD_BASE;
  if (mBase[510] != 0x55 || mBase[511] != 0xaa) { return EFI_VOLUME_CORRUPTED; }
  mMedia.MediaId = 1;
  mMedia.MediaPresent = TRUE;
  mMedia.ReadOnly = TRUE;
  mMedia.BlockSize = 512;
  mMedia.IoAlign = 1;
  mMedia.LastBlock = Info->InitrdSize / 512 - 1;
  DEBUG ((DEBUG_WARN, "cas RAM disk: %Lu bytes at %Lx, read-only\n", Info->InitrdSize, Info->InitrdCopy));
  return gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiBlockIoProtocolGuid, &mBlock,
                                                 &gEfiDevicePathProtocolGuid, &mPath, NULL);
}
