// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_EARLY_TEXT_H
#define CAS_EARLY_TEXT_H
#include <CasDisplay.h>
#include <CasInput.h>

#define CAS_EARLY_TEXT_MAGIC 0x54584543
#define CAS_EARLY_TEXT_VERSION 4
#define CAS_FAULT_ARMED 0x80000000U
#define CAS_FAULT_CONFLICT 0x40000000U
#define CAS_FAULT_ALL 63U
#if defined(CAS_TRACE_RECOVERY_ONLY) || defined(CAS_TRACE_RECOVERY_HOST_TEST)
#define CAS_EARLY_ROWS 24
#define CAS_EARLY_RECOVERY_LOCAL 1
#else
#define CAS_EARLY_ROWS 16
#define CAS_EARLY_TEXT_ADDRESS (CAS_LOG_BASE + 0xc00)
#endif
#define CAS_EARLY_COLS 56

typedef struct {
  CAS_U32 Magic, Version, Size, Row, Column, Busy, Bytes, Frames, Frozen, Match;
  unsigned char Cells[CAS_EARLY_ROWS][CAS_EARLY_COLS];
  CAS_U32 PcMatch, PcPending;
  CAS_U32 FaultState, FaultSlot, FaultMatch, FaultDigits, FaultMask;
  CAS_U64 FaultValues[6]; // SP, ELR, SPSR, FPSR, ESR, FAR; numeric serial data only
  CAS_U32 FaultExceptionMatch;
} CAS_EARLY_TEXT;

// Recovery owns an explicit stack object. The fixed serial-log address is
// deliberately unavailable in that build; it must retain its 16-row ABI.
#ifdef CAS_EARLY_RECOVERY_LOCAL
typedef char CAS_EARLY_TEXT_LOCAL_FITS[(sizeof(CAS_EARLY_TEXT) <= 0x800) ? 1 : -1];
#else
typedef char CAS_EARLY_TEXT_FITS[(sizeof (CAS_EARLY_TEXT) <= 0x400) ? 1 : -1];
typedef char CAS_EARLY_TEXT_AFTER_KEYS[
  (CAS_INPUT_DIAG_ADDRESS + sizeof (CAS_INPUT_DIAG) <= CAS_EARLY_TEXT_ADDRESS) ? 1 : -1];
typedef char CAS_EARLY_TEXT_AFTER_DISPLAY[
  (CAS_DISPLAY_ADDRESS + sizeof (CAS_DISPLAY_STATE) <= CAS_EARLY_TEXT_ADDRESS) ? 1 : -1];
typedef char CAS_EARLY_TEXT_BEFORE_RAW_LOG[
  (CAS_EARLY_TEXT_ADDRESS + sizeof (CAS_EARLY_TEXT) <= CAS_LOG_BASE + 0x1000) ? 1 : -1];

#endif

// Pure bounded text handling and rendering into a supplied framebuffer.
CAS_U32 CasEarlyTextValid (const CAS_EARLY_TEXT *Text);
void CasEarlyTextReset (CAS_EARLY_TEXT *Text);
void CasEarlyTextFeed (CAS_EARLY_TEXT *Text, const unsigned char *Bytes, CAS_U64 Count);
CAS_U32 CasEarlyTextRender (const CAS_EARLY_TEXT *Text, const CAS_FRAMEBUFFER *Frame);
#endif
