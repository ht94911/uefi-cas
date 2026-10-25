// SPDX-License-Identifier: BSD-2-Clause-Patent
#ifndef CAS_XNU_DIAGNOSTICS
#error This mapping is restricted to the diagnostic profile
#endif
#include <CasPlatform.h>
#define GetMemoryMap CasDefaultGetMemoryMap
#include "../MemoryMapLib/MemoryMapLib.c"
#undef GetMemoryMap
STATIC BOOLEAN mApplied=FALSE;
STATIC EFI_MEMORY_REGION_DESCRIPTOR mDiagnosticMap[ARRAY_SIZE(mRegions)+1];
STATIC UINT8 mDiagnosticCount;
VOID GetMemoryMap(EFI_MEMORY_REGION_DESCRIPTOR **Map,UINT8 *Count) {
  UINTN I,Found=0;EFI_MEMORY_REGION_DESCRIPTOR *Default;UINT8 Number;
  CONST EFI_MEMORY_REGION_DESCRIPTOR High={"Diagnostic High Candidate",CAS_DIAG_HIGH_BASE,CAS_DIAG_HIGH_SIZE,
    NoHob,MEM_RES,0,Reserv,ARM_MEMORY_REGION_ATTRIBUTE_WRITE_BACK_XP};
  if(mApplied) {*Map=mDiagnosticMap;*Count=mDiagnosticCount;return;}
  CasDefaultGetMemoryMap(&Default,&Number);*Map=Default;*Count=0;
  if(Number!=ARRAY_SIZE(mRegions) || ARRAY_SIZE(mDiagnosticMap)>=128)return;
  for(I=0;I<Number;++I) {
    EFI_MEMORY_REGION_DESCRIPTOR *R=&Default[I];
    if(!R->Length || R->Address>(~0ULL)-R->Length ||
       (R->Address<CAS_DIAG_HIGH_BASE+CAS_DIAG_HIGH_SIZE && CAS_DIAG_HIGH_BASE<R->Address+R->Length))return;
    if(R->Address==CAS_SMEM_BASE && R->Length==CAS_SMEM_SIZE) {
      if(R->HobOption!=HobOnlyNoCacheSetting || R->ResourceType!=MEM_RES ||
         R->ResourceAttribute!=UNCACHEABLE || R->MemoryType!=Reserv ||
         R->ArmAttributes!=ARM_MEMORY_REGION_ATTRIBUTE_UNCACHED_UNBUFFERED)return;
      ++Found;
    }
  }
  if(Found!=1)return;
  for(I=0;I<Number;++I) {
    mDiagnosticMap[I]=Default[I];
    if(Default[I].Address==CAS_SMEM_BASE)mDiagnosticMap[I].HobOption=AddMem;
  }
  // No resource HOB and no allocation HOB: this maps pages but grants no RAM
  // to DXE. The low heap stays 128MiB until the explicit guarded app action.
  mDiagnosticMap[Number]=High;mDiagnosticCount=Number+1;mApplied=TRUE;
  *Map=mDiagnosticMap;*Count=mDiagnosticCount;
}
