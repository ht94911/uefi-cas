// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_SMEM_CORE_H
#define CAS_SMEM_CORE_H
#include "DiagCore.h"
#define CD_SMEM_BYTES 0x200000U
#define CD_SMEM_TRACE 65536U
#define CD_SMEM_PARTS 32U
typedef int (*CD_SMEM_READ)(void *Context, CD_U32 Offset, CD_U32 Length, CD_U8 *Output);
typedef struct {
  CD_U64 Base, Size, Available;
  CD_U32 Attribute, Category, Domain, Type;
} CD_SMEM_PART;
typedef struct {
  CD_U32 SmemVersion, TableVersion, ItemOffset, ItemSize, Count, Reads, TraceBytes;
  CD_SMEM_PART Parts[CD_SMEM_PARTS];
} CD_SMEM_INFO;
typedef struct {
  CD_U8 Trace[CD_SMEM_TRACE], Table[4096];
  CD_U32 Used, Cursor, Reads, Compare;
  CD_SMEM_READ Read;
  void *Context;
  // Diagnostic-only copy of an already requested metadata read. Item payload
  // and RAM row bytes are never copied here. No additional physical read.
  CD_U32 Check, ReadOffset, ReadLength, SampleBytes;
  CD_U8 Sample[48];
  CD_U32 PartitionOffset, PartitionSize, PartitionFlags, PartitionCacheline;
  CD_U32 ParsedRows, OverlapA, OverlapB;
} CD_SMEM_WORK;
enum { CdSmemValid=0, CdSmemRead, CdSmemFormat, CdSmemVersion, CdSmemMissing,
       CdSmemBounds, CdSmemChanging, CdSmemBudget };
// Read-only observation. Two equal bounded traces are required; no remote lock
// or atomic snapshot is claimed. No pointer from SMEM is dereferenced directly.
int CasDiagSmemRead(CD_SMEM_READ Read, void *Context, CD_SMEM_WORK *Work, CD_SMEM_INFO *Info);
const char *CasDiagSmemCheckName(CD_U32 Check);
// Evidence class only: never UEFI allocation ownership. Domain0 is unspecified.
// Boot/unknown rows are NEVER system candidates and remain full exclusions.
int CasDiagSmemSystemPart(const CD_SMEM_PART *Part);
#endif
