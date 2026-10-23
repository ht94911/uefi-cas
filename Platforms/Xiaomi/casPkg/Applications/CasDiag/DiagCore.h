// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_DIAG_CORE_H
#define CAS_DIAG_CORE_H
// Also compiled as ordinary host C; no UEFI or physical-memory dependencies.
typedef unsigned char CD_U8;
typedef unsigned int CD_U32;
typedef unsigned long long CD_U64;
typedef char CD_WIDTHS[(sizeof(CD_U32)==4 && sizeof(CD_U64)==8) ? 1 : -1];
typedef struct { CD_U64 Base, Size; } CD_RANGE;
typedef struct { CD_U32 Length, Flags; CD_U64 Sequence, BootStamp; CD_U8 Build[32]; } CD_LOG_INFO;
#define CD_LOG_HEADER 96U
#define CD_LOG_CAPACITY 65536U
#define CD_REQUEST 0x10000000ULL
#define CD_ALIGNMENT 0x200000ULL
#define CD_MAX_RANGES 128U
#define CD_FACTS_COMPLETE 15U
#define CD_WARM_REQUESTED 1U
#define CD_PROBE_PASSED 2U
#define CD_PROBE_REFUSED 4U
enum { CdAllowed=0, CdBadRange, CdIncomplete, CdNoFdtBank, CdNoSmemBank, CdNoUefiOwnership, CdProtected };
enum { CdLogValid=0, CdLogMissing, CdLogTruncated, CdLogHeader, CdLogChecksum, CdLogForeign };
int CasDiagRangeValid(CD_RANGE Range, CD_U64 Alignment);
int CasDiagEligible(CD_RANGE Candidate, const CD_RANGE *Banks, CD_U32 BankCount,
  const CD_RANGE *Smem, CD_U32 SmemCount, const CD_RANGE *Free, CD_U32 FreeCount,
  const CD_RANGE *Reserved, CD_U32 ReservedCount, CD_U32 Complete);
CD_U64 CasDiagPattern(CD_U64 Address, CD_U32 Pass);
CD_U32 CasDiagLogEncode(CD_U8 *Output, CD_U32 Capacity, const CD_U8 *Payload,
  CD_U32 Length, CD_U64 Sequence, CD_U64 BootStamp, CD_U32 Flags, const char *Build);
int CasDiagLogValidate(const CD_U8 *Input, CD_U32 Available, const char *Build, CD_LOG_INFO *Info);

typedef struct {
  int (*Allocate)(void *Context, CD_RANGE Range, CD_U64 *Actual);
  int (*Write)(void *Context, CD_RANGE Range, CD_U32 Pass);
  int (*Cache)(void *Context, CD_RANGE Range);
  int (*Read)(void *Context, CD_RANGE Range, CD_U32 Pass);
  int (*Clear)(void *Context, CD_RANGE Range);
  int (*Free)(void *Context, CD_RANGE Range);
} CD_PROBE_IO;
typedef struct { int Result, CleanupFailed, ReleaseFailed; } CD_PROBE_RESULT;
enum { CdProbePass=0, CdProbeDenied, CdProbeAllocation, CdProbeAddress, CdProbeWrite,
  CdProbeCache, CdProbeRead, CdProbeClear, CdProbeRelease };
CD_PROBE_RESULT CasDiagRunProbe(CD_RANGE Range, int Gate, const CD_PROBE_IO *Io, void *Context);

#endif
