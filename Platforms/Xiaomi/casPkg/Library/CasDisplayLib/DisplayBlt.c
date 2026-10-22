// SPDX-License-Identifier: BSD-2-Clause-Patent
// Tested bounded 32-bit GOP blits; preserve the loader's format and opaque alpha.
#include <CasDisplay.h>
#define U64_MAX (~(CAS_U64)0)

static CAS_U32 VideoBounds (const CAS_FRAMEBUFFER *F, CAS_U64 X, CAS_U64 Y, CAS_U64 W, CAS_U64 H) {
  return X <= F->Width && Y <= F->Height && W <= F->Width - X && H <= F->Height - Y;
}

static CAS_U32 BufferBounds (const CAS_U32 *B, CAS_U64 X, CAS_U64 Y, CAS_U64 W, CAS_U64 H, CAS_U64 Pitch) {
  CAS_U64 Rows, End;
  if (B == 0 || ((CAS_U64)B & 3) || Pitch == 0 || (Pitch & 3) || X > Pitch / 4 || W > Pitch / 4 - X ||
      Y > U64_MAX - (H - 1)) { return 0; }
  Rows = Y + H - 1;
  if (Rows > U64_MAX / Pitch) { return 0; }
  End = Rows * Pitch;
  if ((X + W) * 4 > U64_MAX - End) { return 0; }
  End += (X + W) * 4;
  return (CAS_U64)B <= U64_MAX - End;
}

static CAS_U32 ToPixel (const CAS_FRAMEBUFFER *F, CAS_U32 Color) {
  return (((Color >> 16) & 255) << F->RedShift) | (((Color >> 8) & 255) << F->GreenShift) |
         ((Color & 255) << F->BlueShift) | F->ReservedMask;
}

CAS_U32 CasDisplayBlt (const CAS_FRAMEBUFFER *F, CAS_U32 *Buffer, CAS_U32 Op,
  CAS_U64 Sx, CAS_U64 Sy, CAS_U64 Dx, CAS_U64 Dy, CAS_U64 W, CAS_U64 H, CAS_U64 Delta) {
  CAS_U64 Row, Col, Y, X;
  CAS_U32 Color = 0;
  if (F == 0 || W == 0 || H == 0 || Op > 3 || F->Width == 0 || F->Height == 0 ||
      (F->Base & 3) || F->Stride < (CAS_U64)F->Width * 4 || (F->Stride & 3) ||
      F->Bytes < (CAS_U64)F->Stride * F->Height || F->Base > U64_MAX - F->Bytes ||
      F->RedShift > 24 || F->GreenShift > 24 || F->BlueShift > 24) { return 1; }
  if ((Op == 1 || Op == 3) && !VideoBounds (F, Sx, Sy, W, H)) { return 1; }
  if (Op != 1 && !VideoBounds (F, Dx, Dy, W, H)) { return 1; }
  if (Op == 0) {
    if (Buffer == 0 || ((CAS_U64)Buffer & 3)) { return 1; }
    Color = ToPixel (F, Buffer[0]);
  }
  if (Op == 1 || Op == 2) {
    if (!Delta) { if (W > U64_MAX / 4) { return 1; } Delta = W * 4; }
    if (!BufferBounds (Buffer, Op == 1 ? Dx : Sx, Op == 1 ? Dy : Sy, W, H, Delta)) { return 1; }
  }
  for (Row = 0; Row < H; ++Row) {
    volatile CAS_U32 *VideoSource = 0, *VideoDest = 0;
    CAS_U32 *Input = 0, *Output = 0;
    // memmove ordering for overlapping video rectangles, including row padding.
    Y = (Op == 3 && Dy > Sy) ? H - Row - 1 : Row;
    if (Op == 1 || Op == 3) { VideoSource = (void *)(F->Base + (Sy + Y) * F->Stride + Sx * 4); }
    if (Op != 1) { VideoDest = (void *)(F->Base + (Dy + Y) * F->Stride + Dx * 4); }
    if (Op == 1) { Output = (void *)((CAS_U64)Buffer + (Dy + Y) * Delta + Dx * 4); }
    if (Op == 2) { Input = (void *)((CAS_U64)Buffer + (Sy + Y) * Delta + Sx * 4); }
    for (Col = 0; Col < W; ++Col) {
      X = (Op == 3 && Dx > Sx) ? W - Col - 1 : Col;
      if (Op == 0) { VideoDest[X] = Color; }
      else if (Op == 1) {
        CAS_U32 Pixel = VideoSource[X];
        Output[X] = (((Pixel >> F->RedShift) & 255) << 16) | (((Pixel >> F->GreenShift) & 255) << 8) | ((Pixel >> F->BlueShift) & 255);
      } else if (Op == 2) { VideoDest[X] = ToPixel (F, Input[X]); }
      else { VideoDest[X] = VideoSource[X]; }
    }
  }
  return 0;
}
