// SPDX-License-Identifier: BSD-2-Clause-Patent
// Bounded decoder for the exact option emitted by pinned UniXNUDxe.
// Byte reads also work when the load-option fields are unaligned.
#include "CasBootOption.h"
STATIC UINT16 Le16 (CONST UINT8 *P) { return (UINT16)(P[0] | ((UINT16)P[1] << 8)); }
STATIC BOOLEAN Equal (CONST UINT8 *A, CONST UINT8 *B, UINTN N) {
  UINTN I;
  for (I = 0; I < N; ++I) { if (A[I] != B[I]) { return FALSE; } }
  return TRUE;
}
BOOLEAN CasParseUniXnuOption (CONST UINT8 *Data, UINTN Size, CONST EFI_GUID *App,
                             UINTN *PathOffset, UINTN *PathSize) {
  STATIC CONST CHAR16 Description[] = L"UniXNU";
  STATIC CONST CHAR16 Marker[] = L"unixnu:firmware";
  UINTN Offset = 6 + sizeof (Description), Length, End, Node, N;
  BOOLEAN FileSeen = FALSE;
  if (Data == NULL || App == NULL || PathOffset == NULL || PathSize == NULL ||
      Size < Offset + 24 + sizeof (Marker) || Size > 0x2000) { return FALSE; }
  if (Data[0] != 1 || Data[1] != 0 || Data[2] != 0 || Data[3] != 0 ||
      !Equal (Data + 6, (CONST UINT8 *)Description, sizeof (Description))) { return FALSE; }
  Length = Le16 (Data + 4);
  if (Length < 24 || Length > Size - Offset || Size - Offset - Length != sizeof (Marker) ||
      !Equal (Data + Offset + Length, (CONST UINT8 *)Marker, sizeof (Marker))) { return FALSE; }
  End = Offset + Length;
  for (Node = Offset; Node < End; Node += N) {
    if (End - Node < 4) { return FALSE; }
    N = Le16 (Data + Node + 2);
    if (N < 4 || N > End - Node) { return FALSE; }
    if (Data[Node] == 0x7f) {
      if (Data[Node + 1] != 0xff || N != 4 || Node + N != End || !FileSeen) { return FALSE; }
      *PathOffset = Offset; *PathSize = Length; return TRUE;
    }
    if (FileSeen) { return FALSE; }
    if (Data[Node] == 4 && Data[Node + 1] == 6) { // FV_FILEPATH
      if (N != 20 || !Equal (Data + Node + 4, (CONST UINT8 *)App, 16)) { return FALSE; }
      FileSeen = TRUE;
    } else if (!((Data[Node] == 1 && Data[Node + 1] == 3 && N == 24) ||
                 (Data[Node] == 4 && Data[Node + 1] == 7 && N == 20))) {
      // Only memory-mapped / FV prefixes; no hardware disks, URI, file path,
      // alternative instances, or arbitrary BootOrder images.
      return FALSE;
    }
  }
  return FALSE;
}
