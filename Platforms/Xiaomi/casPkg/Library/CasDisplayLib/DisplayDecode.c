// SPDX-License-Identifier: BSD-2-Clause-Patent
// Decode a saved cas/SDE 6.0 snapshot. No pointers from registers are followed.
#include <CasDisplay.h>

static CAS_U32 Staged (const CAS_U32 *Layers, CAS_U32 Pipe, CAS_U32 Virtual) {
  static const CAS_U32 Starts[6] = {0, 3, 6, 26, 18, 21};
  if (Virtual) {
    if (Pipe < 4) { return (Layers[3] >> (Pipe * 4)) & 15; }
    return (Layers[2] >> (8 + (Pipe - 4) * 4)) & 15;
  }
  if (Pipe < 6) {
    CAS_U32 Extra = Pipe < 4 ? Pipe * 2 : 16 + (Pipe - 4) * 2;
    return ((Layers[0] >> Starts[Pipe]) & 7) | ((Layers[1] >> Extra) & 1);
  }
  return (Layers[2] >> ((Pipe - 6) * 4)) & 15;
}

static CAS_U32 RowCovered (const CAS_DISPLAY_SNAPSHOT *S, CAS_U32 Mask, CAS_U32 Y) {
  CAS_U32 X = 0, Next, Pass, P, Px, Py, Pw, Ph;
  for (Pass = 0; Pass < CAS_PIPE_COUNT; ++Pass) {
    Next = X;
    for (P = 0; P < CAS_PIPE_COUNT; ++P) {
      if (!(Mask & (1u << P))) { continue; }
      Px = S->Pipes[P].OutXy & 0xffff; Py = S->Pipes[P].OutXy >> 16;
      Pw = S->Pipes[P].OutSize & 0xffff; Ph = S->Pipes[P].OutSize >> 16;
      if (Py <= Y && Y < Py + Ph && Px <= X && Px + Pw > Next) { Next = Px + Pw; }
    }
    if (Next == CAS_PANEL_WIDTH) { return 1; }
    if (Next == X) { return 0; }
    X = Next;
  }
  return 0;
}

CAS_U32 CasDecodeDisplay (const CAS_DISPLAY_SNAPSHOT *S, CAS_FRAMEBUFFER *R) {
  CAS_U32 C, M, P, I, Mask = 0, CtlMask = 0, Manual = 0, Intfs = 0;
  CAS_U32 Stride = 0, Pattern = 0, First = 1;
  CAS_U64 Base = 0, Bytes = 0;
  unsigned char *Clear = (void *)R;
  if (S == 0 || R == 0) { return CasDisplayRoute; }
  for (I = 0; I < sizeof (*R); ++I) { Clear[I] = 0; }
  if ((S->Revision >> 16) != 0x6000) { return CasDisplayRevision; }
  for (C = 0; C < CAS_CTL_COUNT; ++C) {
    const CAS_CTL_SNAPSHOT *Ctl = &S->Ctls[C];
    CAS_U32 Auto = 0, Selected = 0, Local = 0;
    if (!(Ctl->IntfActive & 6)) { continue; }
    // Only the existing command-mode DSI route is supported. Do not create one.
    if ((Ctl->IntfActive & ~6u) || !(Ctl->Top & (1u << 17))) { return CasDisplayRoute; }
    for (I = 0; I < 2; ++I) {
      CAS_U32 A;
      if (!(Ctl->IntfActive & (2u << I))) { continue; }
      A = S->IntfAutoRefresh[I]; ++Selected;
      if (A & (1u << 31)) {
        if (!(A & 0xffff)) { return CasDisplayRefreshMode; }
        ++Auto;
      }
    }
    if (Auto != 0 && Auto != Selected) { return CasDisplayRefreshMode; }
    for (M = 0; M < CAS_MIXER_COUNT; ++M) {
      for (P = 0; P < CAS_PIPE_COUNT; ++P) {
        if (Staged (Ctl->Layers[M], P, 1)) { return CasDisplayVirtualRect; }
        if (Staged (Ctl->Layers[M], P, 0)) { Local |= 1u << P; }
      }
      // cas has no RGB/cursor pipes; stale unsupported staging is not a match.
      if ((Ctl->Layers[M][0] & ((0x1ffu << 9) | (7u << 29))) ||
          (Ctl->Layers[M][1] & ((0xffu << 8) | (15u << 20) | (15u << 26)))) {
        return CasDisplayRoute;
      }
    }
    if (!Local) { return CasDisplayRoute; }
    CtlMask |= 1u << C; Mask |= Local; Intfs |= Ctl->IntfActive;
    if (!Auto) { Manual |= 1u << C; }
  }
  if (!Mask) { return CasDisplayRoute; }
  for (P = 0; P < CAS_PIPE_COUNT; ++P) {
    const CAS_PIPE_SNAPSHOT *Pipe = &S->Pipes[P];
    CAS_U32 W, H, X, Y, Sx, Sy, Pitch, N, Seen = 0;
    CAS_U64 Input, Output, Candidate, Extent;
    if (!(Mask & (1u << P))) { continue; }
    W = Pipe->OutSize & 0xffff; H = Pipe->OutSize >> 16;
    X = Pipe->OutXy & 0xffff; Y = Pipe->OutXy >> 16;
    Sx = Pipe->Xy & 0xffff; Sy = Pipe->Xy >> 16;
    Pitch = Pipe->Stride & 0xffff;
    if (W == 0 || H == 0 || Pipe->Size != Pipe->OutSize ||
        X + W > CAS_PANEL_WIDTH || Y + H > CAS_PANEL_HEIGHT ||
        Pitch < CAS_PANEL_WIDTH * 4 || (Pitch & 3) || (Pipe->Stride >> 16) ||
        (CAS_U64)(Sx + W) * 4 > Pitch) { return CasDisplayGeometry; }
    if (Pipe->Secure & 15) { return CasDisplaySecure; }
    if (Pipe->Multirect & 7) { return CasDisplayVirtualRect; }
    // Linear, tightly packed, interleaved 8:8:8:8; no scaling/rotation/YUV/UBWC.
    // Source alpha enable is permitted. Its byte is written opaque by the GOP.
    if ((Pipe->Format & ~(1u << 8)) != 0x000236ff ||
        (Pipe->OpMode & ((1u << 0) | (1u << 13) | (1u << 14) | (3u << 22)))) {
      return CasDisplayFormat;
    }
    for (N = 0; N < 4; ++N) {
      CAS_U32 Component = (Pipe->Unpack >> (N * 8)) & 255;
      if (Component > 3 || (Seen & (1u << Component))) { return CasDisplayFormat; }
      Seen |= 1u << Component;
    }
    Input = (CAS_U64)Sy * Pitch + (CAS_U64)Sx * 4;
    Output = (CAS_U64)Y * Pitch + (CAS_U64)X * 4;
    if ((Pipe->Address & 3) || (CAS_U64)Pipe->Address + Input < Output) { return CasDisplayBounds; }
    Candidate = (CAS_U64)Pipe->Address + Input - Output;
    Extent = (CAS_U64)Pitch * CAS_PANEL_HEIGHT;
    if (Candidate < CAS_SPLASH_BASE || Candidate >= (CAS_U64)CAS_SPLASH_BASE + CAS_SPLASH_SIZE ||
        Extent > (CAS_U64)CAS_SPLASH_BASE + CAS_SPLASH_SIZE - Candidate) { return CasDisplayBounds; }
    if (First) { Base = Candidate; Bytes = Extent; Stride = Pitch; Pattern = Pipe->Unpack; First = 0; }
    else if (Base != Candidate || Stride != Pitch || Pattern != Pipe->Unpack) { return CasDisplayInconsistent; }
  }
  // Rectangles must cover the complete panel. Check every horizontal boundary;
  // coverage is constant between consecutive rectangle boundaries.
  if (!RowCovered (S, Mask, 0) || !RowCovered (S, Mask, CAS_PANEL_HEIGHT - 1)) { return CasDisplayCoverage; }
  for (P = 0; P < CAS_PIPE_COUNT; ++P) {
    CAS_U32 Y, End;
    if (!(Mask & (1u << P))) { continue; }
    Y = S->Pipes[P].OutXy >> 16; End = Y + (S->Pipes[P].OutSize >> 16);
    if (!RowCovered (S, Mask, Y) || (End < CAS_PANEL_HEIGHT && !RowCovered (S, Mask, End))) { return CasDisplayCoverage; }
  }
  R->Base = Base; R->Bytes = Bytes; R->Width = CAS_PANEL_WIDTH; R->Height = CAS_PANEL_HEIGHT; R->Stride = Stride;
  for (I = 0; I < 4; ++I) {
    switch ((Pattern >> (I * 8)) & 255) {
      case 0: R->GreenShift = I * 8; R->GreenMask = 255u << (I * 8); break;
      case 1: R->BlueShift = I * 8; R->BlueMask = 255u << (I * 8); break;
      case 2: R->RedShift = I * 8; R->RedMask = 255u << (I * 8); break;
      case 3: R->ReservedMask = 255u << (I * 8); break;
    }
  }
  R->PipeMask = Mask; R->CtlMask = CtlMask; R->ManualCtlMask = Manual; R->IntfMask = Intfs;
  return CasDisplayOk;
}
