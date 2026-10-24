// SPDX-License-Identifier: BSD-2-Clause-Patent
// Original compact diagnostic glyphs; no font assets or firmware blobs.
#include <CasEarlyText.h>

static const unsigned char FreezeMarker[] = "STACK DUMP:";
static const unsigned char PcMarker[] = "PC 0X";
static const unsigned char NestedMarker[] = " EXCEPTION AT 0X";
static const char *FaultLabels[6] = {"SP 0X", "ELR 0X", "SPSR 0X", "FPSR 0X", "ESR 0X", "FAR 0X"};
static const CAS_U32 FaultLengths[6] = {5, 6, 7, 7, 6, 6};

static const struct { unsigned char Code, Rows[7]; } Glyphs[] = {
  {48, {14, 17, 19, 21, 25, 17, 14}},
  {49, {4, 12, 4, 4, 4, 4, 14}},
  {50, {14, 17, 1, 2, 4, 8, 31}},
  {51, {30, 1, 1, 14, 1, 1, 30}},
  {52, {2, 6, 10, 18, 31, 2, 2}},
  {53, {31, 16, 16, 30, 1, 1, 30}},
  {54, {14, 16, 16, 30, 17, 17, 14}},
  {55, {31, 1, 2, 4, 8, 8, 8}},
  {56, {14, 17, 17, 14, 17, 17, 14}},
  {57, {14, 17, 17, 15, 1, 1, 14}},
  {65, {14, 17, 17, 31, 17, 17, 17}},
  {66, {30, 17, 17, 30, 17, 17, 30}},
  {67, {14, 17, 16, 16, 16, 17, 14}},
  {68, {30, 17, 17, 17, 17, 17, 30}},
  {69, {31, 16, 16, 30, 16, 16, 31}},
  {70, {31, 16, 16, 30, 16, 16, 16}},
  {71, {14, 17, 16, 23, 17, 17, 15}},
  {72, {17, 17, 17, 31, 17, 17, 17}},
  {73, {14, 4, 4, 4, 4, 4, 14}},
  {74, {7, 2, 2, 2, 2, 18, 12}},
  {75, {17, 18, 20, 24, 20, 18, 17}},
  {76, {16, 16, 16, 16, 16, 16, 31}},
  {77, {17, 27, 21, 21, 17, 17, 17}},
  {78, {17, 25, 21, 19, 17, 17, 17}},
  {79, {14, 17, 17, 17, 17, 17, 14}},
  {80, {30, 17, 17, 30, 16, 16, 16}},
  {81, {14, 17, 17, 17, 21, 18, 13}},
  {82, {30, 17, 17, 30, 20, 18, 17}},
  {83, {15, 16, 16, 14, 1, 1, 30}},
  {84, {31, 4, 4, 4, 4, 4, 4}},
  {85, {17, 17, 17, 17, 17, 17, 14}},
  {86, {17, 17, 17, 17, 17, 10, 4}},
  {87, {17, 17, 17, 21, 21, 21, 10}},
  {88, {17, 17, 10, 4, 10, 17, 17}},
  {89, {17, 17, 10, 4, 4, 4, 4}},
  {90, {31, 1, 2, 4, 8, 16, 31}},
  {45, {0, 0, 0, 31, 0, 0, 0}},
  {95, {0, 0, 0, 0, 0, 0, 31}},
  {61, {0, 0, 31, 0, 31, 0, 0}},
  {43, {0, 4, 4, 31, 4, 4, 0}},
  {46, {0, 0, 0, 0, 0, 12, 12}},
  {44, {0, 0, 0, 0, 4, 4, 8}},
  {58, {0, 12, 12, 0, 12, 12, 0}},
  {59, {0, 12, 12, 0, 4, 4, 8}},
  {47, {1, 2, 2, 4, 8, 8, 16}},
  {92, {16, 8, 8, 4, 2, 2, 1}},
  {91, {14, 8, 8, 8, 8, 8, 14}},
  {93, {14, 2, 2, 2, 2, 2, 14}},
  {40, {2, 4, 8, 8, 8, 4, 2}},
  {41, {8, 4, 2, 2, 2, 4, 8}},
  {60, {1, 2, 4, 8, 4, 2, 1}},
  {62, {16, 8, 4, 2, 4, 8, 16}},
  {33, {4, 4, 4, 4, 4, 0, 4}},
  {63, {14, 17, 1, 2, 4, 0, 4}},
  {37, {17, 2, 4, 4, 4, 8, 17}},
  {35, {10, 10, 31, 10, 31, 10, 10}},
  {42, {0, 21, 14, 31, 14, 21, 0}},
  {39, {4, 4, 8, 0, 0, 0, 0}},
  {34, {10, 10, 0, 0, 0, 0, 0}},
  {124, {4, 4, 4, 4, 4, 4, 4}},
  {64, {14, 17, 23, 21, 23, 16, 14}},
  {38, {12, 18, 20, 8, 21, 18, 13}},
  {123, {3, 4, 4, 8, 4, 4, 3}},
  {125, {24, 4, 4, 2, 4, 4, 24}},
};

CAS_U32 CasEarlyTextValid (const CAS_EARLY_TEXT *T) {
  return T && T->Magic == CAS_EARLY_TEXT_MAGIC && T->Version == CAS_EARLY_TEXT_VERSION &&
         T->Size == sizeof (*T) && T->Row < CAS_EARLY_ROWS && T->Column <= CAS_EARLY_COLS &&
         T->Frozen <= 1 && T->Match < sizeof (FreezeMarker) - 1 &&
         T->PcMatch <= sizeof (PcMarker) - 1 && T->PcPending <= 1 &&
         T->FaultState <= 4 && T->FaultSlot < 6 && T->FaultMatch <= 8 &&
         (T->FaultState != 1 || T->FaultMatch < FaultLengths[T->FaultSlot]) &&
         T->FaultDigits <= 16 && T->FaultExceptionMatch < sizeof (NestedMarker) - 1 &&
         !(T->FaultMask & ~(CAS_FAULT_ARMED | CAS_FAULT_CONFLICT | CAS_FAULT_ALL));
}

void CasEarlyTextReset (CAS_EARLY_TEXT *T) {
  CAS_U32 I;
  unsigned char *B = (void *)T;
  if (!T) { return; }
  for (I = 0; I < sizeof (*T); ++I) { B[I] = 0; }
  T->Version = CAS_EARLY_TEXT_VERSION;
  T->Size = sizeof (*T);
  T->Magic = CAS_EARLY_TEXT_MAGIC;
}

static void NextLine (CAS_EARLY_TEXT *T) {
  CAS_U32 I;
  T->Row = (T->Row + 1) % CAS_EARLY_ROWS;
  T->Column = 0;
  for (I = 0; I < CAS_EARLY_COLS; ++I) { T->Cells[T->Row][I] = 0; }
}

static void Put (CAS_EARLY_TEXT *T, unsigned char Ch) {
  if (T->Column == CAS_EARLY_COLS) { NextLine (T); }
  T->Cells[T->Row][T->Column++] = Ch;
}

static void FaultNewLine (CAS_EARLY_TEXT *T) {
  T->FaultState = T->FaultSlot = T->FaultMatch = T->FaultDigits = 0;
}

// No printed pointer is dereferenced. Start only at exact source-line prefixes
// "  SP 0x" or " ESR 0x"; commit at most16 hex digits per field, once only.
static void FaultFeed (CAS_EARLY_TEXT *T, unsigned char Ch) {
  CAS_U32 Value;
  unsigned char Upper;
  const char *Label;
  if (!(T->FaultMask & CAS_FAULT_ARMED) || (T->FaultMask & CAS_FAULT_CONFLICT) ||
      (T->FaultMask & CAS_FAULT_ALL) == CAS_FAULT_ALL) { return; }
  // A nested handler before all fields makes association ambiguous. Refuse the
  // later numeric group, rather than pairing its ESR with the first PC.
  Upper = Ch >= 'a' && Ch <= 'z' ? Ch - ('a' - 'A') : Ch;
  if (Upper == NestedMarker[T->FaultExceptionMatch]) {
    if (++T->FaultExceptionMatch == sizeof (NestedMarker) - 1) {
      T->FaultMask |= CAS_FAULT_CONFLICT; T->FaultExceptionMatch = 0; return;
    }
  } else { T->FaultExceptionMatch = Upper == NestedMarker[0] ? 1 : 0; }
  if (Ch == '\r') { return; }
  if (T->FaultState == 2) {
    Value = Ch >= '0' && Ch <= '9' ? Ch - '0' :
            Ch >= 'A' && Ch <= 'F' ? Ch - 'A' + 10 :
            Ch >= 'a' && Ch <= 'f' ? Ch - 'a' + 10 : 16;
    if (Value < 16) {
      if (T->FaultDigits == 16) { T->FaultState = 4; T->FaultValues[T->FaultSlot] = 0; return; }
      T->FaultValues[T->FaultSlot] = (T->FaultValues[T->FaultSlot] << 4) | Value;
      ++T->FaultDigits;
      return;
    }
    if (!T->FaultDigits || (Ch != ' ' && Ch != '\t' && Ch != '\n')) { T->FaultState = 4; }
    else {
      T->FaultMask |= 1U << T->FaultSlot;
      if (T->FaultSlot == 3 || T->FaultSlot == 5) { T->FaultState = 4; }
      else { ++T->FaultSlot; T->FaultState = 3; T->FaultMatch = 0; }
    }
  }
  if (Ch == '\n') { FaultNewLine (T); return; }
  if (T->FaultState == 4) { return; }
  if (Ch >= 'a' && Ch <= 'z') { Ch -= 'a' - 'A'; }
  if (T->FaultState == 0) {
    if (Ch == ' ' && T->FaultMatch < 2) { ++T->FaultMatch; return; }
    if (Ch == 'S' && T->FaultMatch == 2 && !(T->FaultMask & 1)) { T->FaultSlot = 0; }
    else if (Ch == 'E' && T->FaultMatch == 1 && !(T->FaultMask & 16)) { T->FaultSlot = 4; }
    else { T->FaultState = 4; return; }
    T->FaultState = 1; T->FaultMatch = 1;
    return;
  }
  if (T->FaultState == 3) {
    if (Ch == ' ' || Ch == '\t') { return; }
    T->FaultState = 1; T->FaultMatch = 0;
  }
  Label = FaultLabels[T->FaultSlot];
  if (Ch != Label[T->FaultMatch]) { T->FaultState = 4; return; }
  if (++T->FaultMatch == FaultLengths[T->FaultSlot]) {
    if (T->FaultMask & (1U << T->FaultSlot)) { T->FaultState = 4; return; }
    T->FaultState = 2; T->FaultDigits = 0; T->FaultValues[T->FaultSlot] = 0;
  }
}

void CasEarlyTextFeed (CAS_EARLY_TEXT *T, const unsigned char *B, CAS_U64 Count) {
  CAS_U64 I;
  CAS_U32 N;
  unsigned char Ch, Upper;
  if (!T || (!B && Count)) { return; }
  if (!CasEarlyTextValid (T)) { CasEarlyTextReset (T); }
  for (I = 0; I < Count; ++I) {
    Ch = B[I];
    if (T->Frozen) { FaultFeed (T, Ch); continue; }
    if (T->Bytes != ~(CAS_U32)0) { ++T->Bytes; }
    // Capture the first complete handler PC line, preserving its preceding
    // module/load context before X/V registers scroll it away. PcMatch==5 is
    // disabled until a source newline, so display wrapping cannot fake a prefix.
    if (!T->PcPending && T->PcMatch < sizeof (PcMarker) - 1) {
      Upper = (Ch >= 'a' && Ch <= 'z') ? Ch - ('a' - 'A') : Ch;
      if (Upper != PcMarker[T->PcMatch]) { T->PcMatch = sizeof (PcMarker) - 1; }
      else if (++T->PcMatch == sizeof (PcMarker) - 1) { T->PcPending = 1; }
    }
    // The actual pinned handler logs ESR/ELR/FAR before its long stack dump.
    // Latch at the start of that dump, including across serial-write chunks.
    if (T->Match || T->Column == 0) {
      Upper = (Ch >= 'a' && Ch <= 'z') ? Ch - ('a' - 'A') : Ch;
      if (Upper == FreezeMarker[T->Match]) {
        if (++T->Match == sizeof (FreezeMarker) - 1) { T->Frozen = 1; T->Match = 0; }
      } else { T->Match = 0; }
    }
    if (Ch == '\r') { T->Column = 0; }
    else if (Ch == '\n') {
      NextLine (T);
      T->PcMatch = 0;
      if (T->PcPending) { T->Frozen = 1; T->PcPending = 0; T->FaultMask = CAS_FAULT_ARMED; }
    }
    else if (Ch == '\b') {
      if (T->Column) { T->Cells[T->Row][--T->Column] = 0; }
    } else if (Ch == '\t') {
      N = 4 - (T->Column % 4);
      while (N--) { Put (T, ' '); }
    } else if (Ch >= 32) { Put (T, Ch <= 126 ? Ch : '?'); }
    if (T->Frozen && !(T->FaultMask & CAS_FAULT_ARMED)) { break; }
  }
}

static const unsigned char *Glyph (unsigned char Ch) {
  CAS_U32 I;
  if (Ch >= 'a' && Ch <= 'z') { Ch -= 'a' - 'A'; }
  if (!Ch || Ch == ' ') { return 0; }
  for (I = 0; I < sizeof (Glyphs) / sizeof (Glyphs[0]); ++I) {
    if (Glyphs[I].Code == Ch) { return Glyphs[I].Rows; }
  }
  return Glyph ('?');
}

static CAS_U32 AddString (unsigned char *Out, CAS_U32 At, const char *In) {
  while (*In && At < CAS_EARLY_COLS) { Out[At++] = (unsigned char)*In++; }
  return At;
}

static CAS_U32 AddFault (unsigned char *Out, CAS_U32 At, const CAS_EARLY_TEXT *T, CAS_U32 Slot) {
  CAS_U32 I;
  static const char Hex[] = "0123456789ABCDEF";
  for (I = 0; I < 16 && At < CAS_EARLY_COLS; ++I) {
    Out[At++] = T->FaultMask & (1U << Slot) ? Hex[(T->FaultValues[Slot] >> (60 - I * 4)) & 15] : '?';
  }
  return At;
}

static CAS_U32 RenderFault (const CAS_EARLY_TEXT *T, const CAS_FRAMEBUFFER *F, CAS_U32 Y) {
  CAS_U32 Line, Col, R, C, Start, At, A, Black = 0x101820, White = 0xe4edf8;
  unsigned char Text[CAS_EARLY_COLS];
  const unsigned char *G;
  // Separate four-line band above the original16-row loading/first-PC screen.
  if (CasDisplayBlt (F, &Black, 0, 0, 0, 12, Y, CAS_EARLY_COLS * 18, 4 * 24, 0)) { return 1; }
  // Also clear an old banner after reset; never leave stale context visible.
  if (!(T->FaultMask & (CAS_FAULT_ALL | CAS_FAULT_CONFLICT))) { return 0; }
  for (Line = 0; Line < 4; ++Line) {
    for (Col = 0; Col < CAS_EARLY_COLS; ++Col) { Text[Col] = 0; }
    if (Line == 0) {
      AddString (Text, 0, T->FaultMask & CAS_FAULT_CONFLICT ? "FAULT CONTEXT (NESTED; UNBOUND)" :
                 (T->FaultMask & CAS_FAULT_ALL) == CAS_FAULT_ALL ?
                 "FAULT CONTEXT (SERIAL)" : "FAULT CONTEXT (SERIAL; PARTIAL)");
    } else {
      A = Line == 1 ? 0 : Line == 2 ? 4 : 2;
      At = AddString (Text, 0, Line == 1 ? "SP " : Line == 2 ? "ESR " : "SPSR ");
      At = AddFault (Text, At, T, A);
      At = AddString (Text, At, Line == 1 ? " ELR " : Line == 2 ? " FAR " : " FPSR ");
      AddFault (Text, At, T, A + 1);
    }
    for (Col = 0; Col < CAS_EARLY_COLS; ++Col) {
      G = Glyph (Text[Col]);
      if (!G) { continue; }
      for (R = 0; R < 7; ++R) {
        C = 0;
        while (C < 5) {
          if (!(G[R] & (1U << (4 - C)))) { ++C; continue; }
          Start = C++;
          while (C < 5 && (G[R] & (1U << (4 - C)))) { ++C; }
          if (CasDisplayBlt (F, &White, 0, 0, 0, 12 + Col * 18 + Start * 3,
                             Y + Line * 24 + R * 3, (C - Start) * 3, 3, 0)) { return 1; }
        }
      }
    }
  }
  return 0;
}

CAS_U32 CasEarlyTextRender (const CAS_EARLY_TEXT *T, const CAS_FRAMEBUFFER *F) {
  CAS_U32 Line, Col, R, C, Start, Y, SourceRow, White = 0xe4edf8, Black = 0x101820;
  const unsigned char *G;
  // Fixed cas-sized panel; leave both existing stage and fault bands untouched.
  if (!CasEarlyTextValid (T) || !F || F->Width < 24 + CAS_EARLY_COLS * 18 ||
      F->Height < 80 + (CAS_EARLY_ROWS + 4) * 24 + 12) { return 1; }
  Y = F->Height - 80 - CAS_EARLY_ROWS * 24;
  if (CasDisplayBlt (F, &Black, 0, 0, 0, 12, Y,
                     CAS_EARLY_COLS * 18, CAS_EARLY_ROWS * 24, 0)) { return 1; }
  for (Line = 0; Line < CAS_EARLY_ROWS; ++Line) {
    SourceRow = (T->Row + 1 + Line) % CAS_EARLY_ROWS;
    for (Col = 0; Col < CAS_EARLY_COLS; ++Col) {
      G = Glyph (T->Cells[SourceRow][Col]);
      if (!G) { continue; }
      for (R = 0; R < 7; ++R) {
        C = 0;
        while (C < 5) {
          if (!(G[R] & (1u << (4 - C)))) { ++C; continue; }
          Start = C++;
          while (C < 5 && (G[R] & (1u << (4 - C)))) { ++C; }
          if (CasDisplayBlt (F, &White, 0, 0, 0, 12 + Col * 18 + Start * 3,
                             Y + Line * 24 + R * 3, (C - Start) * 3, 3, 0)) { return 1; }
        }
      }
    }
  }
  return RenderFault (T, F, Y - 4 * 24 - 12);
}
