// SPDX-License-Identifier: BSD-2-Clause-Patent
// cas-memory-001. scripts/check_target.py must compare these with memory-map.json.
#ifndef CAS_PLATFORM_H
#define CAS_PLATFORM_H

#define CAS_HEAP_BASE       0x94000000
#define CAS_HEAP_SIZE       0x08000000
#define CAS_FD_BASE         0x9f000000
#define CAS_FD_SIZE         0x00800000
#define CAS_FDT_BASE        0x9f800000
#define CAS_FDT_SIZE        0x00100000
#define CAS_STACK_BASE      0x9f900000
#define CAS_STACK_SIZE      0x00040000
#define CAS_LOG_BASE        0x9f940000
#define CAS_LOG_SIZE        0x00040000
#define CAS_KERNEL_BASE     0xa0000000
#define CAS_KERNEL_SIZE     0x04000000
#define CAS_PAYLOAD_BASE    0xa4000000
#define CAS_PAYLOAD_SIZE    0x04000000
#define CAS_INPUT_END       0xb0000000
#define CAS_TEXT_OFFSET     0x00080000
#define CAS_SPLASH_BASE     0x9c000000
#define CAS_SPLASH_SIZE     0x02300000
#define CAS_GICD_BASE       0x17a00000
#define CAS_GICR_BASE       0x17a60000
#define CAS_TIMER_HZ        19200000
#define CAS_BOOT_MAGIC      0x48534143
#define CAS_BOOT_VERSION    1
#define CAS_BOOT_INFO_SIZE  128

#ifndef __ASSEMBLER__
// No library dependencies: this header is also used by the freestanding shim
// and its host tests. UEFI UINT64 and unsigned long long are both 64 bits here.
typedef struct {
  unsigned int       Magic;
  unsigned int       Version;
  unsigned int       Size;
  unsigned int       Stage;
  unsigned long long KernelBase;
  unsigned long long KernelSize;
  unsigned long long OriginalFdt;
  unsigned long long FdtSize;
  unsigned long long FdtCopy;
  unsigned long long OriginalInitrd;
  unsigned long long InitrdSize;
  unsigned long long InitrdCopy;
  unsigned long long EntryEl;
  unsigned long long EntrySctlr;
  unsigned long long Error;
  unsigned long long Reserved[3];
} CAS_BOOT_INFO;

typedef char CAS_BOOT_INFO_SIZE_CHECK[(sizeof (CAS_BOOT_INFO) == CAS_BOOT_INFO_SIZE) ? 1 : -1];

enum {
  CasStageShimValidated = 1,
  CasStageFdCopied,
  CasStageFdtCopied,
  CasStageSec,
  CasStageMmu,
  CasStageHobs,
  CasStageDecompressed,
  CasStageDxe,
  CasStageGop,
  CasStageKeys,
  CasStageBds,
  CasStageApplication
};

enum {
  CasBootOk = 0,
  CasBootKernelRange = 1,
  CasBootFdtRange = 2,
  CasBootFdtHeader = 3,
  CasBootFdtStructure = 4,
  CasBootReservedOverlap = 5,
  CasBootInitrd = 6,
  CasBootInputOverlap = 7,
  CasBootMemoryMap = 8
};

unsigned int CasValidateBoot (
  unsigned long long KernelBase,
  unsigned long long KernelSize,
  unsigned long long FdtAddress,
  const void         *Fdt,
  CAS_BOOT_INFO      *Info
  );
#endif
#endif
