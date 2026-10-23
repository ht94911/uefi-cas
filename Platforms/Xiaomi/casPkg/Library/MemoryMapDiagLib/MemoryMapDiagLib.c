// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_XNU_DIAGNOSTICS
#error This mapping is restricted to the diagnostic profile
#endif
#include <CasPlatform.h>
#define GetMemoryMap CasDefaultGetMemoryMap
#include "../MemoryMapLib/MemoryMapLib.c"
#undef GetMemoryMap
STATIC BOOLEAN mApplied=FALSE;
VOID GetMemoryMap(EFI_MEMORY_REGION_DESCRIPTOR **Map,UINT8 *Count) {
  UINTN I;UINTN Found=0;
  CasDefaultGetMemoryMap(Map,Count);
  if(mApplied)return;
  for(I=0;I<*Count;++I) {
    EFI_MEMORY_REGION_DESCRIPTOR *R=&(*Map)[I];
    if(R->Address!=CAS_SMEM_BASE || R->Length!=CAS_SMEM_SIZE)continue;
    if(R->HobOption!=HobOnlyNoCacheSetting || R->ResourceType!=MEM_RES ||
       R->ResourceAttribute!=UNCACHEABLE || R->MemoryType!=Reserv ||
       R->ArmAttributes!=ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED) {
      *Count=0;return;
    }
    ++Found;
  }
  if(Found!=1) {*Count=0;return;}
  mApplied=TRUE;
  for(I=0;I<*Count;++I)
    if((*Map)[I].Address==CAS_SMEM_BASE)(*Map)[I].HobOption=AddMem;
}
