// SPDX-License-Identifier: BSD-2-Clause-Patent
#include <Uefi.h>
#include <Guid/EventGroup.h>
#include <Protocol/DevicePath.h>
#include <Protocol/SimpleTextIn.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <CasDisplay.h>
#include <CasInput.h>

#define QUEUE_SIZE 16
STATIC EFI_SIMPLE_TEXT_INPUT_PROTOCOL mInput;
STATIC EFI_EVENT mPollEvent, mExitEvent;
STATIC EFI_INPUT_KEY mQueue[QUEUE_SIZE];
STATIC UINTN mHead, mCount;
STATIC CAS_KEY_STATE mKeys;
STATIC CAS_SPMI_STATE mSpmi;
STATIC UINT32 mFailures;
STATIC CAS_INPUT_DIAG *mDiag = (VOID *)(UINTN)CAS_INPUT_DIAG_ADDRESS;
STATIC struct { VENDOR_DEVICE_PATH Vendor; EFI_DEVICE_PATH_PROTOCOL End; } mPath = {
  {{HARDWARE_DEVICE_PATH, HW_VENDOR_DP, {sizeof (VENDOR_DEVICE_PATH), 0}},
   {0x46d9e6a1, 0x3a0b, 0x4b66, {0xba, 0x7b, 0x57, 0xb9, 0x25, 0x75, 0xda, 0x39}}},
  {END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, {sizeof (EFI_DEVICE_PATH_PROTOCOL), 0}}
};

STATIC CAS_INPUT_U32 Read32 (VOID *Context, CAS_INPUT_U64 Address) { return MmioRead32 ((UINTN)Address); }
STATIC VOID Write32 (VOID *Context, CAS_INPUT_U64 Address, CAS_INPUT_U32 Value) {
  MmioWrite32 ((UINTN)Address, Value); MemoryFence ();
}
STATIC VOID DelayUs (VOID *Context, CAS_INPUT_U32 Us) { MicroSecondDelay (Us); }
STATIC CAS_SPMI_BUS mBus = { Read32, Write32, DelayUs, NULL };

STATIC UINT32 Sample (UINT32 *Raw) {
  UINT32 Pon, Gpio, Result;
  Result = CasSpmiReadByte (&mBus, &mSpmi, CAS_PON_RT_STS, &Pon);
  if (Result == CasSpmiOk) { Result = CasSpmiReadByte (&mBus, &mSpmi, CAS_GPIO6_RT_STS, &Gpio); }
  mDiag->LastStatus = mSpmi.LastStatus;
  if (Result != CasSpmiOk) { ++mDiag->ReadErrors; *Raw = MAX_UINT32; return Result; }
  *Raw = CasKeySample (Pon, Gpio); mDiag->LastRaw = *Raw;
  return CasSpmiOk;
}

STATIC VOID Push (UINT32 Event) {
  EFI_INPUT_KEY Key = {0, 0};
  if (Event == CasKeyFlush) { mHead = 0; mCount = 0; return; }
  if (Event == CasKeyNone) { return; }
  if (Event == CasKeyUp) { Key.ScanCode = SCAN_UP; }
  else if (Event == CasKeyDown) { Key.ScanCode = SCAN_DOWN; }
  else if (Event == CasKeyEscape) { Key.ScanCode = SCAN_ESC; }
  else if (Event == CasKeyEnter) { Key.UnicodeChar = L'\r'; }
  else { return; }
  if (mCount == QUEUE_SIZE) { ++mDiag->Dropped; return; }
  mQueue[(mHead + mCount) % QUEUE_SIZE] = Key; ++mCount; ++mDiag->Events;
  gBS->SignalEvent (mInput.WaitForKey);
}

STATIC VOID EFIAPI Poll (EFI_EVENT Event, VOID *Context) {
  EFI_TPL Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  UINT32 Raw, Result;
  UINT64 Now = GetTimeInNanoSecond (GetPerformanceCounter ()) / 1000000;
  Result = Sample (&Raw);
  if (Result != CasSpmiOk) {
    if (++mFailures == 1) { DEBUG ((DEBUG_ERROR, "cas keys read failed: code=%u status=%x address=%x\n", Result, mSpmi.LastStatus, mSpmi.LastAddress)); }
    if (mFailures == 5) { gBS->SetTimer (mPollEvent, TimerPeriodic, 1000000); }
  } else if (mFailures) {
    DEBUG ((DEBUG_WARN, "cas keys recovered after %u failed samples\n", mFailures));
    mFailures = 0; gBS->SetTimer (mPollEvent, TimerPeriodic, CAS_KEY_POLL_100NS);
  }
  Push (CasKeysUpdate (&mKeys, Raw, Now));
  gBS->RestoreTPL (Tpl);
}

STATIC EFI_STATUS EFIAPI Reset (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, BOOLEAN ExtendedVerification) {
  EFI_TPL Tpl;
  if (This != &mInput) { return EFI_INVALID_PARAMETER; }
  Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  mHead = 0; mCount = 0; ZeroMem (&mKeys, sizeof (mKeys));
  gBS->RestoreTPL (Tpl);
  return EFI_SUCCESS;
}

STATIC EFI_STATUS EFIAPI ReadKey (EFI_SIMPLE_TEXT_INPUT_PROTOCOL *This, EFI_INPUT_KEY *Key) {
  EFI_TPL Tpl;
  if (This != &mInput || Key == NULL) { return EFI_INVALID_PARAMETER; }
  Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  if (!mCount) { gBS->RestoreTPL (Tpl); return mFailures ? EFI_DEVICE_ERROR : EFI_NOT_READY; }
  *Key = mQueue[mHead]; mHead = (mHead + 1) % QUEUE_SIZE; --mCount;
  gBS->RestoreTPL (Tpl);
  return EFI_SUCCESS;
}

STATIC VOID EFIAPI Wait (EFI_EVENT Event, VOID *Context) {
  EFI_TPL Tpl = gBS->RaiseTPL (TPL_NOTIFY);
  if (mCount) { gBS->SignalEvent (Event); }
  gBS->RestoreTPL (Tpl);
}
STATIC VOID EFIAPI Stop (EFI_EVENT Event, VOID *Context) {
  if (mPollEvent != NULL) { gBS->CloseEvent (mPollEvent); mPollEvent = NULL; }
}

EFI_STATUS EFIAPI CasKeysInitialize (EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE *SystemTable) {
  EFI_HANDLE Handle = NULL;
  EFI_STATUS Status;
  UINT32 Result, Raw, I, Value;
  ZeroMem (mDiag, sizeof (*mDiag)); mDiag->Magic = CAS_INPUT_DIAG_MAGIC; mDiag->Version = 1;
  Result = CasSpmiInitialize (&mBus, &mSpmi);
  mDiag->SpmiVersion = mSpmi.Version; mDiag->PonApid = mSpmi.PonApid; mDiag->GpioApid = mSpmi.GpioApid;
  mDiag->InitStatus = Result;
  DEBUG ((DEBUG_WARN, "cas SPMI: version=%08x init=%u PON APID=%u GPIO6 APID=%u\n", mSpmi.Version, Result, mSpmi.PonApid, mSpmi.GpioApid));
  if (Result != CasSpmiOk) { return EFI_UNSUPPORTED; }
  Result = Sample (&Raw); mDiag->InitStatus = Result;
  if (Result != CasSpmiOk) { return EFI_DEVICE_ERROR; }
  // Record PON reset settings left by the boot chain, without changing them.
  for (I = 0; I < 12; ++I) {
    Result = CasSpmiReadByte (&mBus, &mSpmi, 0x840 + I, &Value);
    mDiag->ResetConfig[I] = Result == CasSpmiOk ? Value : (0x80000000u | Result);
    DEBUG ((DEBUG_WARN, "cas PON reset register %04x = %08x\n", 0x840 + I, mDiag->ResetConfig[I]));
  }
  mInput.Reset = Reset; mInput.ReadKeyStroke = ReadKey;
  Status = gBS->CreateEvent (EVT_NOTIFY_WAIT, TPL_CALLBACK, Wait, NULL, &mInput.WaitForKey);
  if (EFI_ERROR (Status)) { return Status; }
  Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_CALLBACK, Poll, NULL, &mPollEvent);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->CreateEventEx (EVT_NOTIFY_SIGNAL, TPL_NOTIFY, Stop, NULL, &gEfiEventExitBootServicesGuid, &mExitEvent);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->SetTimer (mPollEvent, TimerPeriodic, CAS_KEY_POLL_100NS);
  if (EFI_ERROR (Status)) { goto Failure; }
  Status = gBS->InstallMultipleProtocolInterfaces (&Handle, &gEfiDevicePathProtocolGuid, &mPath, &gEfiSimpleTextInProtocolGuid, &mInput, NULL);
  if (EFI_ERROR (Status)) { goto Failure; }
  ((CAS_BOOT_INFO *)(UINTN)CAS_LOG_BASE)->Stage = CasStageKeys;
  CasDisplayMark (CasStageKeys);
  return EFI_SUCCESS;
Failure:
  if (mExitEvent != NULL) { gBS->CloseEvent (mExitEvent); mExitEvent = NULL; }
  if (mPollEvent != NULL) { gBS->CloseEvent (mPollEvent); mPollEvent = NULL; }
  gBS->CloseEvent (mInput.WaitForKey); mInput.WaitForKey = NULL;
  return Status;
}
