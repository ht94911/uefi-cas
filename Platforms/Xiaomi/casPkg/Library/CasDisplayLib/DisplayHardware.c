// SPDX-License-Identifier: BSD-2-Clause-Patent
// Shared by the freestanding shim and EDK modules. No bootloader service calls.
#include <CasDisplay.h>

static const CAS_U32 PipeOffsets[CAS_PIPE_COUNT] = {0x5000,0x7000,0x9000,0xb000,0x25000,0x27000,0x29000,0x2b000};
static const CAS_U32 CtlOffsets[CAS_CTL_COUNT] = {0x2000,0x2200,0x2400,0x2600,0x2800,0x2a00};
static const CAS_U32 IntfOffsets[2] = {0x6b800,0x6c000};
static const CAS_U32 PpOffsets[6] = {0x71000,0x71800,0x72000,0x72800,0x73000,0x73800};

static CAS_U32 Read (CAS_U32 Offset) {
  CAS_U32 Value = *(volatile CAS_U32 *)(CAS_U64)(CAS_MDP_BASE + Offset);
  __asm__ __volatile__ ("dmb sy" ::: "memory");
  return Value;
}

CAS_DISPLAY_STATE *CasDisplayState (void) { return (CAS_DISPLAY_STATE *)(CAS_U64)CAS_DISPLAY_ADDRESS; }

CAS_U32 CasDisplayInitialize (void) {
  CAS_DISPLAY_STATE *State = CasDisplayState ();
  const CAS_BOOT_INFO *Info = (void *)(CAS_U64)CAS_LOG_BASE;
  CAS_DISPLAY_SNAPSHOT *S = &State->Snapshot;
  CAS_U32 I, M, B;
  unsigned char *Bytes = (void *)State;
  if (Info->Magic != CAS_BOOT_MAGIC || Info->Version != CAS_BOOT_VERSION) { return CasDisplayRoute; }
  if (State->Magic == CAS_DISPLAY_MAGIC && State->Version == CAS_DISPLAY_VERSION && State->Size == sizeof (*State)) { return State->Error; }
  for (I = 0; I < sizeof (*State); ++I) { Bytes[I] = 0; }
  State->Magic = CAS_DISPLAY_MAGIC; State->Version = CAS_DISPLAY_VERSION; State->Size = sizeof (*State);
  S->Revision = Read (0);
  if ((S->Revision >> 16) != 0x6000) { State->Error = CasDisplayRevision; return State->Error; }
  for (I = 0; I < CAS_PIPE_COUNT; ++I) {
    CAS_PIPE_SNAPSHOT *P = &S->Pipes[I]; B = PipeOffsets[I];
    P->Size = Read (B); P->Xy = Read (B + 0x08); P->OutSize = Read (B + 0x0c); P->OutXy = Read (B + 0x10);
    P->Address = Read (B + 0x14); P->Stride = Read (B + 0x24); P->Format = Read (B + 0x30);
    P->Unpack = Read (B + 0x34); P->OpMode = Read (B + 0x38); P->Secure = Read (B + 0x70); P->Multirect = Read (B + 0x170);
  }
  for (I = 0; I < CAS_CTL_COUNT; ++I) {
    CAS_CTL_SNAPSHOT *C = &S->Ctls[I]; B = CtlOffsets[I];
    C->Top = Read (B + 0x14); C->IntfActive = Read (B + 0xf4); C->Start = Read (B + 0x1c); C->Flush = Read (B + 0x18);
    for (M = 0; M < CAS_MIXER_COUNT; ++M) {
      C->Layers[M][0] = Read (B + (M == 5 ? 0x24 : M * 4));
      C->Layers[M][1] = Read (B + 0x40 + M * 4); C->Layers[M][2] = Read (B + 0x70 + M * 4);
      C->Layers[M][3] = Read (B + 0xa0 + M * 4);
    }
  }
  for (I = 0; I < 2; ++I) { S->IntfAutoRefresh[I] = Read (IntfOffsets[I] + 0x2b4); S->IntfTearEnable[I] = Read (IntfOffsets[I] + 0x284); }
  for (I = 0; I < 6; ++I) { S->PpAutoRefresh[I] = Read (PpOffsets[I] + 0x30); }
  State->Error = CasDecodeDisplay (S, &State->Framebuffer);
  State->Ready = State->Error == CasDisplayOk;
  __asm__ __volatile__ ("dsb sy" ::: "memory");
  return State->Error;
}

void CasDisplayRefresh (void) {
  CAS_DISPLAY_STATE *State = CasDisplayState ();
  CAS_U32 I, B;
  if (State->Magic != CAS_DISPLAY_MAGIC || !State->Ready || State->Stopped) { return; }
  // The framebuffer mapping is NC before/after MMU setup; publish CPU stores.
  __asm__ __volatile__ ("dsb sy" ::: "memory");
  for (I = 0; I < CAS_CTL_COUNT; ++I) {
    if (!(State->Framebuffer.ManualCtlMask & (1u << I))) { continue; }
    B = CtlOffsets[I];
    if (Read (B + 0xf4) != State->Snapshot.Ctls[I].IntfActive || Read (B + 0x14) != State->Snapshot.Ctls[I].Top) {
      ++State->RefreshErrors; State->Stopped = 1; return;
    }
    if (Read (B + 0x1c) & 1) { ++State->BusyCount; continue; }
    // Only memory content changed: no CTL_FLUSH or display reconfiguration.
    *(volatile CAS_U32 *)(CAS_U64)(CAS_MDP_BASE + B + 0x1c) = 1;
    __asm__ __volatile__ ("dsb sy" ::: "memory");
    ++State->RefreshCount;
  }
}

static void Rectangle (const CAS_FRAMEBUFFER *F, CAS_U32 X, CAS_U32 Y, CAS_U32 W, CAS_U32 H, CAS_U32 Rgb) {
  CAS_U32 Row, Col, Color;
  if (X > F->Width || Y > F->Height || W > F->Width - X || H > F->Height - Y) { return; }
  Color = (((Rgb >> 16) & 255) << F->RedShift) | (((Rgb >> 8) & 255) << F->GreenShift) |
          ((Rgb & 255) << F->BlueShift) | F->ReservedMask;
  for (Row = Y; Row < Y + H; ++Row) {
    volatile CAS_U32 *Dest = (void *)(F->Base + (CAS_U64)Row * F->Stride + X * 4);
    for (Col = 0; Col < W; ++Col) { Dest[Col] = Color; }
  }
}

static void HexDigit (const CAS_FRAMEBUFFER *F, CAS_U32 X, CAS_U32 Y, CAS_U32 Digit, CAS_U32 Color) {
  // Five 3-bit rows. A compact original diagnostic font for 0..F.
  static const CAS_U32 Glyphs[16] = {0x7b6f,0x2492,0x73e7,0x73cf,0x5bc9,0x79cf,0x79ef,0x7249,0x7bef,0x7bcf,0x7bed,0x6bae,0x7927,0x6b6e,0x79e7,0x79e4};
  CAS_U32 Row, Col;
  for (Row = 0; Row < 5; ++Row) {
    for (Col = 0; Col < 3; ++Col) {
      if (Glyphs[Digit & 15] & (1u << ((4 - Row) * 3 + 2 - Col))) { Rectangle (F, X + Col * 3, Y + Row * 3, 3, 3, Color); }
    }
  }
}

void CasDisplayMark (CAS_U32 Stage) {
  CAS_DISPLAY_STATE *S = CasDisplayState ();
  CAS_U32 I, Step;
  if (S->Magic != CAS_DISPLAY_MAGIC || !S->Ready || S->Stopped || Stage == 0 || Stage > CasStageApplication) { return; }
  S->Stages |= 1u << Stage; S->LastStage = Stage;
  Step = S->Framebuffer.Width / CasStageApplication;
  for (I = 1; I <= CasStageApplication; ++I) {
    CAS_U32 Color = (S->Stages & (1u << I)) ? 0x16a070 : 0x303540;
    Rectangle (&S->Framebuffer, (I - 1) * Step + 1, S->Framebuffer.Height - 28, Step - 2, 27, Color);
    HexDigit (&S->Framebuffer, (I - 1) * Step + 8, S->Framebuffer.Height - 23, I, 0xffffff);
  }
  CasDisplayRefresh ();
}

void CasDisplayFault (CAS_U64 Error) {
  CAS_DISPLAY_STATE *S = CasDisplayState ();
  CAS_U32 I;
  if (S->Magic != CAS_DISPLAY_MAGIC || !S->Ready || S->Stopped) { return; }
  Rectangle (&S->Framebuffer, 0, S->Framebuffer.Height - 60, 300, 30, 0xa00000);
  for (I = 0; I < 16; ++I) { HexDigit (&S->Framebuffer, 8 + I * 16, S->Framebuffer.Height - 53, (CAS_U32)(Error >> ((15 - I) * 4)), 0xffffff); }
  CasDisplayRefresh ();
}

void CasDisplayStop (void) { CasDisplayState ()->Stopped = 1; }
