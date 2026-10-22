// SPDX-License-Identifier: BSD-2-Clause-Patent
// Bounded RAM log. The display renderer is added separately after handoff audit.
#include <Library/BaseLib.h>
#include <Library/SerialPortLib.h>
#include <CasPlatform.h>

#define CAS_LOG_HEADER_SIZE  16
#define CAS_LOG_DATA_SIZE    (CAS_LOG_SIZE - 0x1000 - CAS_LOG_HEADER_SIZE)
typedef struct {
  UINT32 Magic;
  UINT32 Used;
  UINT32 Dropped;
  UINT32 Reserved;
  UINT8  Data[CAS_LOG_DATA_SIZE];
} CAS_RAM_LOG;

STATIC volatile CAS_RAM_LOG *Log (VOID) {
  return (volatile CAS_RAM_LOG *)(UINTN)(CAS_LOG_BASE + 0x1000);
}

RETURN_STATUS EFIAPI SerialPortInitialize (VOID) {
  volatile CAS_BOOT_INFO *Info = (VOID *)(UINTN)CAS_LOG_BASE;
  if (Info->Magic != CAS_BOOT_MAGIC || Info->Version != CAS_BOOT_VERSION) {
    return RETURN_DEVICE_ERROR;
  }
  if (Log ()->Magic != SIGNATURE_32 ('C', 'L', 'O', 'G')) {
    Log ()->Used = 0;
    Log ()->Dropped = 0;
    Log ()->Magic = SIGNATURE_32 ('C', 'L', 'O', 'G');
  }
  return RETURN_SUCCESS;
}

UINTN EFIAPI SerialPortWrite (UINT8 *Buffer, UINTN NumberOfBytes) {
  UINTN Index;
  BOOLEAN Interrupts;
  volatile CAS_RAM_LOG *Current = Log ();
  if (Buffer == NULL || RETURN_ERROR (SerialPortInitialize ())) { return 0; }
  Interrupts = GetInterruptState ();
  DisableInterrupts ();
  for (Index = 0; Index < NumberOfBytes; ++Index) {
    if (Current->Used < CAS_LOG_DATA_SIZE) {
      Current->Data[Current->Used++] = Buffer[Index];
    } else if (Current->Dropped < MAX_UINT32) {
      ++Current->Dropped;
    }
  }
  MemoryFence ();
  if (Interrupts) { EnableInterrupts (); }
  return NumberOfBytes;
}

UINTN EFIAPI SerialPortRead (UINT8 *Buffer, UINTN NumberOfBytes) { return 0; }
BOOLEAN EFIAPI SerialPortPoll (VOID) { return FALSE; }
RETURN_STATUS EFIAPI SerialPortSetControl (UINT32 Control) { return RETURN_UNSUPPORTED; }
RETURN_STATUS EFIAPI SerialPortGetControl (UINT32 *Control) {
  if (Control == NULL) { return RETURN_INVALID_PARAMETER; }
  *Control = EFI_SERIAL_INPUT_BUFFER_EMPTY | EFI_SERIAL_OUTPUT_BUFFER_EMPTY;
  return RETURN_SUCCESS;
}
RETURN_STATUS EFIAPI SerialPortSetAttributes (UINT64 *BaudRate, UINT32 *ReceiveFifoDepth, UINT32 *Timeout,
  EFI_PARITY_TYPE *Parity, UINT8 *DataBits, EFI_STOP_BITS_TYPE *StopBits) { return RETURN_UNSUPPORTED; }
