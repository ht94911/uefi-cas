// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <CasDisplay.h>

STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL mGop;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE mMode;
STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION mInfo;
STATIC EFI_EVENT mRefreshEvent, mExitEvent;
STATIC struct {
  VENDOR_DEVICE_PATH Vendor;
  EFI_DEVICE_PATH_PROTOCOL End;
} mPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof (VENDOR_DEVICE_PATH), 0}},
   {0x2424e1aa, 0x2407, 0x48cf, {0xb4, 0xab, 0x95, 0x20, 0x62, 0x44, 0xb0, 0xbb}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {sizeof (EFI_DEVICE_PATH_PROTOCOL), 0}}
};

STATIC EFI_STATUS EFIAPI Query (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode,
  UINTN *Size, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **Info) {
  if (This != &mGop || Size == NULL || Info == NULL) { return EFI_INVALID_PARAMETER; }
  if (Mode != 0) { return EFI_UNSUPPORTED; }
  *Info = AllocateCopyPool (sizeof (mInfo), &mInfo);
  if (*Info == NULL) { return EFI_OUT_OF_RESOURCES; }
  *Size = sizeof (mInfo);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI Blt (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, EFI_GRAPHICS_OUTPUT_BLT_PIXEL *Buffer,
  EFI_GRAPHICS_OUTPUT_BLT_OPERATION Op, UINTN Sx, UINTN Sy, UINTN Dx, UINTN Dy, UINTN W, UINTN H, UINTN Delta) {
  EFI_TPL Tpl;
  CAS_U32 Result;
  if (This != &mGop || Op >= EfiGraphicsOutputBltOperationMax) { return EFI_INVALID_PARAMETER; }
  Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  Result = CasDisplayBlt (&CasDisplayState ()->Framebuffer, (CAS_U32 *)Buffer, Op, Sx, Sy, Dx, Dy, W, H, Delta);
  if (Result == 0 && Op != EfiBltVideoToBltBuffer) { CasDisplayRefresh (); }
  gBS->RestoreTPL (Tpl);
  return Result == 0 ? EFI_SUCCESS : EFI_INVALID_PARAMETER;
}

STATIC EFI_STATUS EFIAPI SetMode (EFI_GRAPHICS_OUTPUT_PROTOCOL *This, UINT32 Mode) {
  EFI_GRAPHICS_OUTPUT_BLT_PIXEL Black = {0, 0, 0, 0};
  if (This != &mGop) { return EFI_INVALID_PARAMETER; }
  if (Mode != 0) { return EFI_UNSUPPORTED; }
  return Blt (This, &Black, EfiBltVideoFill, 0, 0, 0, 0, mInfo.HorizontalResolution, mInfo.VerticalResolution, 0);
}

STATIC VOID EFIAPI Refresh (EFI_EVENT Event, VOID *Context) { CasDisplayRefresh (); }
STATIC VOID EFIAPI Stop (EFI_EVENT Event, VOID *Context) {
  CasDisplayStop ();
  if (mRefreshEvent != NULL) { gBS->CloseEvent (mRefreshEvent); mRefreshEvent = NULL; }
}

STATIC VOID LogSnapshot (CONST CAS_DISPLAY_STATE *S) {
  UINTN I;
  DEBUG ((DEBUG_WARN, "cas display: SDE=%08x decode=%u ready=%u\n", S->Snapshot.Revision, S->Error, S->Ready));
  for (I = 0; I < CAS_PIPE_COUNT; ++I) {
    CONST CAS_PIPE_SNAPSHOT *P = &S->Snapshot.Pipes[I];
    DEBUG ((DEBUG_WARN, "SSPP%u: addr=%08x size=%08x xy=%08x out=%08x/%08x stride=%08x fmt=%08x unpack=%08x op=%08x\n",
      (UINT32)I, P->Address, P->Size, P->Xy, P->OutSize, P->OutXy, P->Stride, P->Format, P->Unpack, P->OpMode));
  }
  for (I = 0; I < CAS_CTL_COUNT; ++I) {
    DEBUG ((DEBUG_WARN, "CTL%u: top=%08x intf=%08x start=%08x flush=%08x\n", (UINT32)I,
      S->Snapshot.Ctls[I].Top, S->Snapshot.Ctls[I].IntfActive, S->Snapshot.Ctls[I].Start, S->Snapshot.Ctls[I].Flush));
  }
}

EFI_STATUS EFIAPI CasGopInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  CAS_DISPLAY_STATE *State;
  CAS_FRAMEBUFFER *F;
  CasDisplayInitialize ();
  State = CasDisplayState (); LogSnapshot (State);
  if (!State->Ready || State->Error != CasDisplayOk) { return EFI_UNSUPPORTED; }
  F = &State->Framebuffer;
  mInfo.Version = 0; mInfo.HorizontalResolution = F->Width; mInfo.VerticalResolution = F->Height;
  mInfo.PixelsPerScanLine = F->Stride / 4;
  mInfo.PixelFormat = PixelBitMask;
  mInfo.PixelInformation.RedMask = F->RedMask; mInfo.PixelInformation.GreenMask = F->GreenMask;
  mInfo.PixelInformation.BlueMask = F->BlueMask; mInfo.PixelInformation.ReservedMask = F->ReservedMask;
  if (F->BlueMask == 0xff && F->RedMask == 0xff0000) { mInfo.PixelFormat = PixelBlueGreenRedReserved8BitPerColor; }
  else if (F->RedMask == 0xff && F->BlueMask == 0xff0000) { mInfo.PixelFormat = PixelRedGreenBlueReserved8BitPerColor; }
  mMode.MaxMode = 1; mMode.Mode = 0; mMode.Info = &mInfo; mMode.SizeOfInfo = sizeof (mInfo);
  mMode.FrameBufferBase = F->Base; mMode.FrameBufferSize = (UINTN)F->Bytes;
  mGop.QueryMode = Query; mGop.SetMode = SetMode; mGop.Blt = Blt; mGop.Mode = &mMode;
  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Refresh, NULL, &mRefreshEvent);
  if (EFI_ERROR (Status)) { return Status; }
  // Also refresh after a blit while CTL was busy or an EFI client writes directly.
  Status = gBS->SetTimer (mRefreshEvent, TimerPeriodic, 200000);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Stop, NULL, &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiDevicePathProtocolGuid, &mPath,
                                                  &gEfiGraphicsOutputProtocolGuid, &mGop, NULL);
  if (EFI_ERROR (Status)) { gBS->CloseEvent (mExitEvent); mExitEvent = NULL; goto Failure; }
  ((CAS_BOOT_INFO *)(UINTN)CAS_LOG_BASE)->Stage = CasStageGop;
  CasDisplayMark (CasStageGop);
  DEBUG ((DEBUG_WARN, "cas GOP: %ux%u base=%Lx size=%Lu stride=%u pipes=%x ctls=%x manual=%x\n",
          F->Width, F->Height, F->Base, F->Bytes, F->Stride, F->PipeMask, F->CtlMask, F->ManualCtlMask));
  return EFI_SUCCESS;
Failure:
  gBS->CloseEvent (mRefreshEvent); mRefreshEvent = NULL;
  return Status;
}
