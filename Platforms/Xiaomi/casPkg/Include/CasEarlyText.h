// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_EARLY_TEXT_H
#define CAS_EARLY_TEXT_H
#include <CasDisplay.h>
#include <CasInput.h>

#define CAS_EARLY_TEXT_ADDRESS (CAS_LOG_BASE + 0xc00)
#define CAS_EARLY_TEXT_MAGIC 0x54584543
#define CAS_EARLY_TEXT_VERSION 2
#define CAS_EARLY_ROWS 16
#define CAS_EARLY_COLS 56

typedef struct {
  CAS_U32 Magic, Version, Size, Row, Column, Busy, Bytes, Frames, Frozen, Match;
  unsigned char Cells[CAS_EARLY_ROWS][CAS_EARLY_COLS];
} CAS_EARLY_TEXT;

typedef char CAS_EARLY_TEXT_FITS[(sizeof (CAS_EARLY_TEXT) <= 0x400) ? 1 : -1];
typedef char CAS_EARLY_TEXT_AFTER_KEYS[
  (CAS_INPUT_DIAG_ADDRESS + sizeof (CAS_INPUT_DIAG) <= CAS_EARLY_TEXT_ADDRESS) ? 1 : -1];
typedef char CAS_EARLY_TEXT_AFTER_DISPLAY[
  (CAS_DISPLAY_ADDRESS + sizeof (CAS_DISPLAY_STATE) <= CAS_EARLY_TEXT_ADDRESS) ? 1 : -1];
typedef char CAS_EARLY_TEXT_BEFORE_RAW_LOG[
  (CAS_EARLY_TEXT_ADDRESS + sizeof (CAS_EARLY_TEXT) <= CAS_LOG_BASE + 0x1000) ? 1 : -1];

// Pure bounded text handling and rendering into a supplied framebuffer.
CAS_U32 CasEarlyTextValid (const CAS_EARLY_TEXT *Text);
void CasEarlyTextReset (CAS_EARLY_TEXT *Text);
void CasEarlyTextFeed (CAS_EARLY_TEXT *Text, const unsigned char *Bytes, CAS_U64 Count);
CAS_U32 CasEarlyTextRender (const CAS_EARLY_TEXT *Text, const CAS_FRAMEBUFFER *Frame);
#endif
