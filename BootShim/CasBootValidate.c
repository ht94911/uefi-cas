// SPDX-License-Identifier: BSD-2-Clause-Patent
// Pure bounded validation shared by the freestanding cas shim and host tests.
// The ASM caller validates its own code/stack corridor before calling C.
#include <CasPlatform.h>

typedef unsigned int U32;
typedef unsigned long long U64;
typedef unsigned char U8;

static U32 Be32 (const U8 *P) {
  return ((U32)P[0] << 24) | ((U32)P[1] << 16) | ((U32)P[2] << 8) | P[3];
}

static U64 Be64 (const U8 *P) {
  return ((U64)Be32 (P) << 32) | Be32 (P + 4);
}

static int Fits (U64 Base, U64 Size, U64 Low, U64 High) {
  return Base >= Low && Base <= High && Size <= High - Base;
}

static int Overlap (U64 A, U64 An, U64 B, U64 Bn) {
  // All users first establish non-overflowing input ranges.
  return An != 0 && Bn != 0 && A < B + Bn && B < A + An;
}

static int DestinationOverlap (U64 Base, U64 Size) {
  if (Size > ~Base) {
    return 1;
  }
  return Overlap (Base, Size, CAS_HEAP_BASE, CAS_HEAP_SIZE) ||
         Overlap (Base, Size, CAS_FD_BASE, CAS_FD_SIZE) ||
         Overlap (Base, Size, CAS_FDT_BASE, CAS_FDT_SIZE) ||
         Overlap (Base, Size, CAS_STACK_BASE, CAS_STACK_SIZE) ||
         Overlap (Base, Size, CAS_LOG_BASE, CAS_LOG_SIZE) ||
         Overlap (Base, Size, CAS_PAYLOAD_BASE, CAS_PAYLOAD_SIZE);
}

static int NameIs (const U8 *Name, U32 Available, const char *Expected) {
  U32 N = 0;
  while (N < Available && Expected[N] != 0 && Name[N] == (U8)Expected[N]) {
    ++N;
  }
  return N < Available && Name[N] == 0 && Expected[N] == 0;
}

static int MemoryName (const U8 *Name, U32 Available) {
  const char Prefix[] = "memory";
  U32 N;
  if (Available < 7) {
    return 0;
  }
  for (N = 0; N < 6; ++N) {
    if (Name[N] != (U8)Prefix[N]) {
      return 0;
    }
  }
  return Name[6] == 0 || Name[6] == '@';
}

static int CoversDestinations (U64 Base, U64 Size) {
  // All destinations are within this captured lower RAM bank, with gaps kept.
  return Size <= ~Base && Fits (CAS_HEAP_BASE, CAS_HEAP_SIZE, Base, Base + Size) &&
    Fits (CAS_FD_BASE, (U64)CAS_LOG_BASE + CAS_LOG_SIZE - CAS_FD_BASE, Base, Base + Size) &&
    Fits (CAS_KERNEL_BASE, (U64)CAS_INPUT_END - CAS_KERNEL_BASE, Base, Base + Size);
}

U32 CasValidateBoot (U64 KernelBase, U64 KernelSize, U64 FdtAddress,
                     const void *Fdt, CAS_BOOT_INFO *Info) {
  const U8 *Tree = (const U8 *)Fdt;
  U32 Total, Struct, StructSize, Strings, StringsSize, Reserve;
  U32 Cursor, End, Depth = 0, ChosenDepth = 0, ReservedDepth = 0, MemoryDepth = 0;
  U32 StartSeen = 0, EndSeen = 0, EndToken = 0, RootSeen = 0, MemorySeen = 0;
  U32 RootAddressCells = 0, RootSizeCells = 0, ReservedAddressCells = 0, ReservedSizeCells = 0;
  U64 InitrdStart = 0, InitrdEnd = 0;

  if ((KernelBase & 0xfff) != 0 || KernelSize < CAS_FD_SIZE ||
      !Fits (KernelBase, KernelSize, CAS_KERNEL_BASE, (U64)CAS_KERNEL_BASE + CAS_KERNEL_SIZE)) {
    return CasBootKernelRange;
  }
  if (Tree == 0 || Info == 0 || (FdtAddress & 7) != 0 ||
      !Fits (FdtAddress, 40, CAS_KERNEL_BASE, CAS_INPUT_END)) {
    return CasBootFdtRange;
  }
  Total = Be32 (Tree + 4);
  if (Be32 (Tree) != 0xd00dfeed || Total < 40 || Total > CAS_FDT_SIZE ||
      !Fits (FdtAddress, Total, CAS_KERNEL_BASE, CAS_INPUT_END)) {
    return CasBootFdtHeader;
  }
  if (Overlap (KernelBase, KernelSize, FdtAddress, Total)) {
    return CasBootInputOverlap;
  }
  Struct = Be32 (Tree + 8);
  Strings = Be32 (Tree + 12);
  Reserve = Be32 (Tree + 16);
  StringsSize = Be32 (Tree + 32);
  StructSize = Be32 (Tree + 36);
  if (Be32 (Tree + 20) != 17 || Be32 (Tree + 24) > 17 || Struct < 40 || Strings < 40 || Reserve < 40 ||
      (Struct & 3) != 0 || (Reserve & 7) != 0 || StructSize < 16 || (StructSize & 3) != 0 ||
      !Fits (Struct, StructSize, 40, Total) || !Fits (Strings, StringsSize, 40, Total) ||
      !Fits (Reserve, 16, 40, Total) || Overlap (Struct, StructSize, Strings, StringsSize)) {
    return CasBootFdtHeader;
  }
  Cursor = Reserve;
  for (;;) {
    U64 Base, Size;
    if (!Fits (Cursor, 16, 40, Total) || Overlap (Cursor, 16, Struct, StructSize) ||
        Overlap (Cursor, 16, Strings, StringsSize)) {
      return CasBootFdtHeader;
    }
    Base = Be64 (Tree + Cursor);
    Size = Be64 (Tree + Cursor + 8);
    Cursor += 16;
    if (Base == 0 && Size == 0) {
      break;
    }
    if (DestinationOverlap (Base, Size)) {
      return CasBootReservedOverlap;
    }
  }

  Cursor = Struct;
  End = Struct + StructSize;
  while (Fits (Cursor, 4, Struct, End)) {
    U32 Token = Be32 (Tree + Cursor);
    Cursor += 4;
    if (Token == 1) { // FDT_BEGIN_NODE
      U32 NameStart = Cursor;
      while (Cursor < End && Tree[Cursor] != 0) {
        ++Cursor;
      }
      if (Cursor == End || Depth >= 64 || (Depth == 0 && (RootSeen || Cursor != NameStart))) {
        return CasBootFdtStructure;
      }
      if (Depth == 0) {
        RootSeen = 1;
      }
      ++Depth;
      if (Depth == 2 && NameIs (Tree + NameStart, End - NameStart, "chosen")) {
        if (ChosenDepth) { return CasBootFdtStructure; }
        ChosenDepth = Depth;
      }
      if (Depth == 2 && NameIs (Tree + NameStart, End - NameStart, "reserved-memory")) {
        if (ReservedDepth) { return CasBootFdtStructure; }
        ReservedDepth = Depth;
      }
      if (Depth == 2 && MemoryName (Tree + NameStart, End - NameStart)) {
        MemoryDepth = Depth;
      }
      Cursor = (Cursor + 4) & ~3U;
    } else if (Token == 2) { // FDT_END_NODE
      if (Depth == 0) { return CasBootFdtStructure; }
      if (Depth == ChosenDepth) { ChosenDepth = 0; }
      if (Depth == ReservedDepth) { ReservedDepth = 0; }
      if (Depth == MemoryDepth) { MemoryDepth = 0; }
      --Depth;
    } else if (Token == 3) { // FDT_PROP
      U32 Length, NameOffset, NameEnd;
      const U8 *Name, *Value;
      if (Depth == 0 || !Fits (Cursor, 8, Struct, End)) { return CasBootFdtStructure; }
      Length = Be32 (Tree + Cursor);
      NameOffset = Be32 (Tree + Cursor + 4);
      Cursor += 8;
      if (!Fits (Cursor, Length, Struct, End) || NameOffset >= StringsSize) { return CasBootFdtStructure; }
      NameEnd = NameOffset;
      while (NameEnd < StringsSize && Tree[Strings + NameEnd] != 0) { ++NameEnd; }
      if (NameEnd == StringsSize) { return CasBootFdtStructure; }
      Name = Tree + Strings + NameOffset;
      Value = Tree + Cursor;
      if (Depth == 1 && NameIs (Name, StringsSize - NameOffset, "#address-cells")) {
        if (Length != 4 || RootAddressCells) { return CasBootFdtStructure; }
        RootAddressCells = Be32 (Value);
      }
      if (Depth == 1 && NameIs (Name, StringsSize - NameOffset, "#size-cells")) {
        if (Length != 4 || RootSizeCells) { return CasBootFdtStructure; }
        RootSizeCells = Be32 (Value);
      }
      if (ReservedDepth && Depth == ReservedDepth) {
        if (NameIs (Name, StringsSize - NameOffset, "#address-cells")) {
          if (Length != 4) { return CasBootFdtStructure; }
          ReservedAddressCells = Be32 (Value);
        }
        if (NameIs (Name, StringsSize - NameOffset, "#size-cells")) {
          if (Length != 4) { return CasBootFdtStructure; }
          ReservedSizeCells = Be32 (Value);
        }
      }
      if (ChosenDepth && Depth == ChosenDepth) {
        int IsStart = NameIs (Name, StringsSize - NameOffset, "linux,initrd-start");
        int IsEnd = NameIs (Name, StringsSize - NameOffset, "linux,initrd-end");
        if (IsStart || IsEnd) {
          U64 Address;
          if (Length != 4 && Length != 8) { return CasBootInitrd; }
          Address = Length == 4 ? Be32 (Value) : Be64 (Value);
          if (IsStart) { if (StartSeen++) { return CasBootInitrd; } InitrdStart = Address; }
          if (IsEnd) { if (EndSeen++) { return CasBootInitrd; } InitrdEnd = Address; }
        }
      }
      if (NameIs (Name, StringsSize - NameOffset, "reg")) {
        if (ReservedDepth && Depth == ReservedDepth + 1) {
          U32 Index;
          if (ReservedAddressCells != 2 || ReservedSizeCells != 2 || Length == 0 || Length % 16 != 0) {
            return CasBootFdtStructure;
          }
          for (Index = 0; Index < Length; Index += 16) {
            if (DestinationOverlap (Be64 (Value + Index), Be64 (Value + Index + 8))) {
              return CasBootReservedOverlap;
            }
          }
        }
        if (MemoryDepth && Depth == MemoryDepth) {
          U32 Index;
          if (RootAddressCells != 2 || RootSizeCells != 2 || Length == 0 || Length % 16 != 0) {
            return CasBootMemoryMap;
          }
          for (Index = 0; Index < Length; Index += 16) {
            if (CoversDestinations (Be64 (Value + Index), Be64 (Value + Index + 8))) { MemorySeen = 1; }
          }
        }
      }
      Cursor = (Cursor + Length + 3) & ~3U;
    } else if (Token == 4) { // FDT_NOP
      continue;
    } else if (Token == 9) { // FDT_END
      if (Depth != 0 || !RootSeen) { return CasBootFdtStructure; }
      EndToken = 1;
      break;
    } else {
      return CasBootFdtStructure;
    }
  }
  if (!EndToken || RootAddressCells != 2 || RootSizeCells != 2 || !MemorySeen) {
    return CasBootMemoryMap;
  }
  if (StartSeen != EndSeen || InitrdEnd < InitrdStart || InitrdEnd - InitrdStart > CAS_PAYLOAD_SIZE ||
      (InitrdEnd != InitrdStart && !Fits (InitrdStart, InitrdEnd - InitrdStart, CAS_KERNEL_BASE, CAS_INPUT_END))) {
    return CasBootInitrd;
  }
  if (Overlap (InitrdStart, InitrdEnd - InitrdStart, KernelBase, KernelSize) ||
      Overlap (InitrdStart, InitrdEnd - InitrdStart, FdtAddress, Total)) {
    return CasBootInputOverlap;
  }
  for (Cursor = 0; Cursor < sizeof (*Info); ++Cursor) { ((U8 *)Info)[Cursor] = 0; }
  Info->Magic = CAS_BOOT_MAGIC;
  Info->Version = CAS_BOOT_VERSION;
  Info->Size = sizeof (*Info);
  Info->Stage = CasStageShimValidated;
  Info->KernelBase = KernelBase;
  Info->KernelSize = KernelSize;
  Info->OriginalFdt = FdtAddress;
  Info->FdtSize = Total;
  Info->FdtCopy = CAS_FDT_BASE;
  Info->OriginalInitrd = InitrdStart;
  Info->InitrdSize = InitrdEnd - InitrdStart;
  Info->InitrdCopy = CAS_PAYLOAD_BASE;
  return CasBootOk;
}
